# Population continuation
Objective: preserve individual fauna state and ongoing routes through native world saves.
Acceptance: exact continuous-vs-restored domain evolution; backward-compatible STL7 and STL1..6 read support; strict malformed input and identity validation; native save/load replay restores actor projection and resumes movement.
Ownership: isolated worker owns fauna_population.hpp, persistence.hpp, their tests and fauna docs. Root owns host, scripts, integration and validation.
Budget: native gpt-6-luna medium; bounded20min; no network or credentials. Worktree var/worktrees/fauna-continuation-20261017.
