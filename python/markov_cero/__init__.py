# markov-cero Python package (W7/D-11).
# The compiled extension module is markov_cero._core (built by setup.py /
# CMake); this package re-exports it as the public `markov_cero` surface.
from ._core import *          # noqa: F401,F403
from ._core import __doc__    # noqa: F401
