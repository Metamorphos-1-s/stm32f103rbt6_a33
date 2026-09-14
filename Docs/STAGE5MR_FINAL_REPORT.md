# Stage 5M-R Final Report

Status: `STAGE 5M-R NO SAFE DRIFT CANDIDATE; REAL LOAD PRESERVATION REQUIREMENT NOT MET`.

Requirements and labels were corrected without changing Stage 5M-A history. A fixed-point conservative candidate and offline replay were implemented, but the candidate reversely amplified one real static control. Hardware shadow mode was therefore not built or run. Product Release, 40 Hz containment and all public semantics remain unchanged.

Stage 5M-F, Stage 5N and Stage 5O were not entered.

Software gates pass: Host CTest 19/19; Stage5B 3/3 plus 30/30; Stage5C 12/12; Stage5L 8/8; G2 8/8; Stage5M-A 9/9; Stage5M-R 5/5; Python/C 1000/1000 exact. Debug, Release, BoardDiagnostics and Stage5LDiagnostics clean builds pass. The isolated ARM candidate is 1528 bytes text with 72-byte Process stack. Clang, ASan and UBSan are NOT RUN because supported tooling is unavailable.

Product Release SHA-256 remains `82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486` and contains no StaticDriftCompensator symbol. No hardware access, diagnostic build, flash, physical action or configuration write occurred in Stage 5M-R.
