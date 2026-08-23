#!/usr/bin/env python3
"""worldforge_caller -- Gloamstead's side of a WorldForge world-generation request.

Gloamstead is the IMPORTING GAME. WorldForge is a capability engine that lives in
a different repository and knows nothing about this project. Everything in this
package is Gloamstead stating facts about itself and stating what it wants; none
of it decides how a world is made, because that is not Gloamstead's job and Core
must stay reusable by the next caller.

The one field that makes this real is ``provenance.origination ==
"caller_originated"`` in :mod:`gloamstead_consumer`. WorldForge ships
demonstration consumers it wrote itself; those declare
``worldforge_authored_demonstration`` for their whole lives. This one does not,
because Gloamstead authored the intent it carries.

Nothing here imports Unreal, and nothing here mutates the project. It is a
declaration that a generic WorldForge runner can read.
"""

__all__ = ["gloamstead_consumer"]
