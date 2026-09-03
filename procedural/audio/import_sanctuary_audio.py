"""
Import the forged sanctuary audio into Gloamstead, headless.

Run with:
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript
        -script="procedural/audio/import_sanctuary_audio.py" -unattended -nullrhi -nosplash -nopause
    (the WAV source directory is read from GLOAM_AUDIO_WAV_DIR)

The counterpart to forge_sanctuary_audio.py, and the direct sibling of
procedural/textures/import_sanctuary_textures.py -- same shape, same contract: a deterministic
generator writes source files, this script turns them into .uassets the game loads, and no editor
GUI is involved at any step.

WHY THE LOOPING FLAG IS SET HERE AND NOT LEFT TO THE CALLER
    USoundWave::bLooping is a property of the *asset*, not of the thing playing it. A 20-second bed
    imported without it plays once and leaves the sanctuary silent for the rest of the phase, and
    that failure is invisible in the content browser -- the wave auditions perfectly, because
    auditioning one pass is exactly what it is supposed to do. So the flag is stated per asset from
    the same table that named the file, and then read back off the saved package rather than
    trusted, because a set_editor_property that did not stick reports nothing at all.

    The three one-shots are asserted non-looping for the same reason in reverse: a warning tone that
    loops is not a slightly worse warning tone, it is a stuck alarm.

WHY THE FORMAT IS VERIFIED AND NOT ASSUMED
    Unreal will happily import a wave at any rate and silently resample it for the platform's mixer.
    The forge writes 48 kHz mono specifically because that is what the project's mixer runs at; if a
    future edit to the generator changes that, the resample is a quality loss nobody would notice
    until it was baked into a cook. So sample rate, channel count and duration are read back off
    each saved asset and checked against what the forge said it wrote.

Destination is /Game/Gloamstead/Audio. Nothing here writes outside it, and nothing here touches
vendor content or any existing package.
"""
import os

import unreal

WAV_DIR = os.environ.get("GLOAM_AUDIO_WAV_DIR", "")
DEST = "/Game/Gloamstead/Audio"

EXPECTED_SAMPLE_RATE = 48000
EXPECTED_CHANNELS = 1

# source stem -> (should_loop, expected_duration_seconds, human role)
#
# The durations are the generator's, not a guess: forge_sanctuary_audio.py builds the four beds at
# AMBIENT_SECONDS = 20 and the one-shots at 2.5 / 2.0 / 1.5. Restating them here is deliberate
# duplication -- it is what turns "the import worked" into "the import brought in the file I meant".
ASSETS = [
    ("AMB_Sanctuary_Day",   True,  20.0, "ambient bed (Day)"),
    ("AMB_Sanctuary_Dusk",  True,  20.0, "ambient bed (Dusk)"),
    ("AMB_Sanctuary_Night", True,  20.0, "ambient bed (Night)"),
    ("AMB_Sanctuary_Dawn",  True,  20.0, "ambient bed (Dawn)"),
    ("SFX_Heart_Warning",   False,  2.5, "one-shot (Heart warning)"),
    ("SFX_Restoration",     False,  2.0, "one-shot (Restoration)"),
    ("SFX_Threat_Near",     False,  1.5, "one-shot (Threat near)"),
]

DURATION_TOLERANCE = 0.02  # seconds; a whole-sample-count import should be exact, this is slack


def asset_name(stem):
    """S_ prefix, matching the repo's T_ / SM_ / MI_ convention for its own generated content."""
    return "S_" + stem


def build_task(path, name, factory):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    # Stated rather than inherited: without an explicit factory the importer picks one by extension,
    # which is right today and is exactly the kind of implicit behaviour that changes under you.
    task.factory = factory
    # Saved explicitly after bLooping is applied, not here -- saving at import time would persist the
    # importer's default (non-looping) and the corrected value would then be a second, unnecessary
    # revision of the package.
    task.save = False
    return task


def main():
    if not WAV_DIR or not os.path.isdir(WAV_DIR):
        raise RuntimeError("GLOAM_AUDIO_WAV_DIR is unset or not a directory: %r" % WAV_DIR)

    # One factory per task, held in a list for the duration of the import. A factory that is garbage
    # collected between construction and use takes its task's import with it.
    factories = []
    tasks = []
    for stem, _loop, _dur, _role in ASSETS:
        path = os.path.join(WAV_DIR, stem + ".wav")
        if not os.path.exists(path):
            raise RuntimeError("missing forged source: %s (run forge_sanctuary_audio.py first)" % path)
        factory = unreal.SoundFactory()
        factories.append(factory)
        tasks.append(build_task(path, asset_name(stem), factory))

    print("AUDIMPORT: importing %d WAV(s) from %s into %s" % (len(tasks), WAV_DIR, DEST))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    failures = []

    for stem, should_loop, _dur, _role in ASSETS:
        name = asset_name(stem)
        obj_path = "%s/%s" % (DEST, name)
        asset = unreal.EditorAssetLibrary.load_asset(obj_path)
        if asset is None:
            failures.append("%s did not load after import" % name)
            continue
        if not isinstance(asset, unreal.SoundWave):
            failures.append("%s imported as %s, not a SoundWave" % (name, type(asset).__name__))
            continue
        asset.set_editor_property("looping", should_loop)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

    # Verify by re-loading from disk, not by trusting the setters above.
    for stem, should_loop, want_duration, role in ASSETS:
        name = asset_name(stem)
        obj_path = "%s/%s" % (DEST, name)
        asset = unreal.EditorAssetLibrary.load_asset(obj_path)
        if asset is None:
            failures.append("%s did not re-load after save" % name)
            continue

        got_loop = bool(asset.get_editor_property("looping"))
        duration = float(asset.get_editor_property("duration"))
        channels = int(asset.get_editor_property("num_channels"))
        rate = int(asset.get_editor_property("sample_rate"))

        if got_loop != should_loop:
            failures.append("%s looping is %r, expected %r" % (name, got_loop, should_loop))
        if abs(duration - want_duration) > DURATION_TOLERANCE:
            failures.append("%s duration is %.4fs, expected %.4fs" % (name, duration, want_duration))
        if channels != EXPECTED_CHANNELS:
            failures.append("%s has %d channel(s), expected %d" % (name, channels, EXPECTED_CHANNELS))
        if rate != EXPECTED_SAMPLE_RATE:
            failures.append("%s is %d Hz, expected %d Hz" % (name, rate, EXPECTED_SAMPLE_RATE))

        print("AUDIMPORT: %-24s %-26s %7.4fs  %d ch  %d Hz  looping=%s"
              % (name, role, duration, channels, rate, got_loop))

    if failures:
        raise RuntimeError("AUDIMPORT FAILED: " + "; ".join(failures))
    print("AUDIMPORT: complete, %d sound wave(s) under %s" % (len(ASSETS), DEST))


main()
