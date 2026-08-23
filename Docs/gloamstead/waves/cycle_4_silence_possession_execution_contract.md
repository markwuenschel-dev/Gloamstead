# Cycle 4: Silence Possession

This slice is the fourth authored night in the Gloamstead progression. It is a consequence on the
restored garden, not a generic enemy wave.

| Beat | Player-readable contract |
| --- | --- |
| Warning | The Heart repeats `GardenRot` for `SilencePossession`, but changes the fragment: “The garden goes silent beneath borrowed light.” |
| Evidence | The same place supplies three channels: vines stop moving (environment), soil stays cold (object reaction), and bell moths fall silent (audio). Any two distinct channels are enough to earn the interpretation receipt. |
| Preparation | Restore the authored `Cycle2_Garden` GardenBed and bring the light back to it. The night will never substitute another restored point. |
| Consequence | At night start the restored garden becomes occupied. Pressure raises corruption on that point and the cosmetic pressure presence follows the exact target. |
| Survival | The player presses the light-ward action twice: the first ward disrupts the hold, the second purifies it. A single ward is a visible Partial; no ward is a Failure that leaves a corruption scar. |
| Dawn | `PossessionPurified`, `PossessionDisrupted`, `PossessionScar`, or `PossessionNoTarget` is recorded as the cycle outcome. The next plan is armed from the versioned authored save state. |

The implementation is intentionally fail-closed: an absent or ambiguous authored target produces a
quiet `PossessionNoTarget` outcome rather than punishing a different place. `RMB` and `GloamWard`
route through the same runtime authority, so keyboard play and automation exercise the same action.
