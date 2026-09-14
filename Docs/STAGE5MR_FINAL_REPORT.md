# Stage 5M-R Final Report

Status: `STAGE 5M-R NO SAFE DRIFT CANDIDATE; REAL LOAD PRESERVATION REQUIREMENT NOT MET`.

Requirements and labels were corrected without changing Stage 5M-A history. A fixed-point conservative candidate and offline replay were implemented, but the candidate reversely amplified one real static control. Hardware shadow mode was therefore not built or run. Product Release, 40 Hz containment and all public semantics remain unchanged.

Stage 5M-F, Stage 5N and Stage 5O were not entered.

Software gates pass: Host CTest 19/19; Stage5B 3/3 plus 30/30; Stage5C 12/12; Stage5L 8/8; G2 8/8; Stage5M-A 9/9; Stage5M-R 5/5; Python/C 1000/1000 exact. Debug, Release, BoardDiagnostics and Stage5LDiagnostics clean builds pass. The isolated ARM candidate is 1528 bytes text with 72-byte Process stack. Clang, ASan and UBSan are NOT RUN because supported tooling is unavailable.

Product Release SHA-256 remains `82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486` and contains no StaticDriftCompensator symbol. No hardware access, diagnostic build, flash, physical action or configuration write occurred in Stage 5M-R.

STM32 started at `304709b3457bfb78bf94daf64ee389d4921b6597`. Commits are `d6c03a1` for requirements, fixed-point candidate, replay and tests; `3639be0` for immutable offline failure evidence; and `2ca7b96` for the Git-blob Manifest V2. The report-only tip follows. Client remains unchanged at `c4e4906f0a47a427793df6cfcb414756ac7984cc`; CH579 remains unchanged at `eb888925e4fcc9dcd9bf89e8cc42e5b28679e520`.

The Manifest passes local and clean checkout validation under `core.autocrlf=false`, `input` and `true`. No merge, tag, PR, force push, rebase or history rewrite was performed. Product integration is not started, 40 Hz remains contained, and Stage 5M-F/5N/5O remain pending.
