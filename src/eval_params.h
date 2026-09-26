#pragma once

// Overwritten by tools/tune (see docs/TUNING.md). Until a tuning run has
// produced values, the evaluator starts from its built-in defaults.
constexpr bool HAVE_TUNED_PARAMS = false;
constexpr int TUNED_PARAMS[1][2] = {{0, 0}};
