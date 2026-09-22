Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The Korg MS-50, the MS-50 name, and the circuit designs of that instrument are the property of Korg Inc. Martial Systems LLC claims copyright only in the original text of this repository and in any code later written here. The work is an independent study of published schematics and of the literature cited in the research summary. Korg has not produced, sponsored, or endorsed it. No license is granted to the MS-50 design, to the Korg drawings, or to the Korg trademarks. The instrument's name is used only to identify the subject of the study.

# Test plan

Date: 2026-09-21.

Automated tests live in `Source/Tests/GraphTests.cpp` and are run with the `MS50ModularTests` target from step 2 on. Host checks are done in a real plugin host after the step that needs them. This environment's design pack does not include a host, so a step that says "host" is not done until a person or an agent with a host has done it and written the host name.

Stand-in ids are defined in `METHODOLOGY.md`. A test that encodes a stand-in number must name the id in a comment on the assertion.

## Plugin loads

After step 1:

*   VST3 builds from the CMake command in the build guide.
*   A host scans it as an effect, not an instrument.
*   Stereo track, stereo out. A mono track is not a supported layout (`isBusesLayoutSupported` returns false).
*   Playing MIDI notes does not change audio. MIDI input is off.
*   Bypass in the host returns dry stereo, subject to the host's own bypass. This is a host feature, not an Output-module test.

## Dry Ext In to Output

After step 3, and again after steps 6, 8, and 18:

*   Cables: Ext In L to Output L, Ext In R to Output R. Mix 0.
*   Left-only signal stays left. Right-only stays right.
*   A file recorded through the plugin at mix 0 matches the source within a residual below -90 dBFS, delay 0 samples.
*   Mix 1 with Wet unpatched is silence.
*   `testDryMixPassesStereo` and `testExtInMonoAveragesStereo` pass.

## Each module

Run the named tests when the matching build step lands. "Finite" means every sample in a 1 second render at 48 kHz is finite and inside ±40 V in the graph (well above the ±5 V stand-in, tight enough to catch a blow-up).

| Module | Tests | Extra check |
|---|---|---|
| Ext In | `testExtInMonoAveragesStereo`, `testExtInGateFiresAboveThreshold`, `testExtInButtonForcesGate` | Button is on the Ext In faceplate, not a hidden key |
| Output | `testDryMixPassesStereo`, `testWetMixIgnoresDry`, `testLevelZeroIsSilence` | Stereo image collapses only as mix rises |
| Graph | `testOneCablePerInput`, `testFanOutAllowed`, `testRejectSignalIntoGate`, `testRejectCycle` until step 19, then `testFeedbackIsOneSample` | Second cable does not replace the first |
| Noise | `testNoiseBothJacksMove`, `testNoiseSeedRepeats`, `testNoisePinkIsDarkerThanWhite`, `testNoiseHasNoKnobs` | No level control on the panel |
| VCF step 8 | `testVcfPassesDcOrLow`, `testVcfPeakIncreasesResonance`, `testVcfPositiveCvRaisesCutoff` | Peak at 1 stays finite. No highpass switch |
| VCF step 20 | `testVcfStaysFiniteWhenDrivenHard`, `testVcfHotInputMovesSpectrum` | Listening note `docs/listening/step-20.md` exists. No constant 250 |
| VCA 1 | `testVca1SilentWithoutEnv`, `testVca1IntensityScalesOutput`, `testVca1LowCutDarkens`, `testVca1NegativeEnvIsClosed` | Unpatched env is silence, not unity |
| VCA 2 | `testVca2PassesDc`, `testVca2ControlDoesNotClick`, `testVca2NoKnobs` | Different class from VCA 1 |
| EG 1 | `testEg1SustainLevel`, `testEg1OutBIsNegation`, `testEg1ReleasesWhenTriggerLifts`, `testEg1HasDecay`, `testEg1ThreeJacks`, `testEg1UnpatchedTrigIsIdle` | Three jacks visible. A missing trig cable does not hold the envelope |
| MG | `testMgPulseIsUnipolar`, `testMgTriangleIsBipolar2V5`, `testMgFreqEndpoints`, `testMgPwAffectsPulseAndTriangle`, `testMgSawJacksOpposite` | Faceplate says MG. Pulse never goes below 0 V |
| VCO | `testScaleDoesNotChangeOctJack`, `testOctIsOneVoltPerOctave`, `testHzPerVoltIsLinear`, `testThreeOutputsAlwaysRun`, `testPwmMovesDutyNotPitch` | Unpatched Hz/V still sounds at the footage pitch |
| EG 2 | `testEg2HasNoSustainKnob`, `testEg2ReturnsToZeroWithoutAPlateau`, `testEg2DelayTrigAfterHold`, `testEg2NegIsNegation`, `testEg2Restart` | Faceplate has no decay and no sustain |
| Ring | `testRingFourQuadrant`, `testRingZeroKills`, `testRingPassesDcProduct`, `testRingHasNoKnobs` | Product, not a sum |
| Divider | `testDividerOnlyTwoAndFour`, `testDividerSquareCounts`, `testDividerIgnoresTinySignal` | 100 input edges, 50 and 25 output edges |
| Inverter | `testInverterNegatesDc`, `testInverterNegatesAudio`, `testInverterHasNoKnobs` | +3 V becomes -3 V |
| Integrator | `testIntegratorSettlesToInput`, `testIntegratorSameSign`, `testIntegratorSlowIsSlower`, `testIntegratorIsItsOwnModule` | Still a faceplate after every UI step |

Default patch smoke, after step 10 and again after step 20:

*   Eight cables from `SCHEMATICS.md` are present.
*   Silence in, mix 1: silence out (VCA 1 closed).
*   A 220 Hz sine at -12 dBFS, Ext In button held: signal appears after the attack and dies after release.
*   Integrator faceplate is visible while this runs.

## Illegal patches

| Attempt | Result |
|---|---|
| Second cable into Output Wet | Rejected. First cable stays. Status string on the message thread |
| Audio or CV into EG 2 DelayTrig, which is Gate | Rejected |
| Gate into VCA 1 Env, which is CV | Allowed. Held gate writes 0 V (S-15), so the VCA closes while the gate is held. This is surprising and must be stated in the status line the first time it happens: "gate patched as active-low voltage" |
| Cable that closes a cycle, before step 19 | Rejected |
| Cable that closes a cycle, after step 19 | Accepted. Newest cable delayed one sample |
| Cable from a jack to the same jack | Rejected |
| Disconnect of a missing cable | No-op, no crash |

UI and graph must agree. A test calls the same `connect` the mouse-up handler calls.

## Preset recall

After step 18:

*   `testPresetRoundTrip` and `testPresetRejectsBadVersion`.
*   Host: save a project with a non-default cutoff and one extra cable. Reopen. Knobs and cables match. EG phase does not need to match. Noise seed does not need to match.
*   A corrupt blob does not crash `setStateInformation`. The previous in-memory patch remains.
*   Version field is an integer at the start of the block.

## CPU smoke

After step 20, in a host, 48 kHz, buffer 64:

*   Default patch, silence in, button not held, for 30 seconds. The audio thread stays free of allocations (plugin built with a debug allocator counter, or Instruments/Time Profiler showing no `operator new` on the audio thread).
*   Default patch, -12 dBFS stereo noise in, button held, peak at 1, for 30 seconds. No dropouts on a machine that can run an empty VST3 at that buffer. This is a smoke test, not a cycle budget.
*   Drag cables for 10 seconds while audio runs. No crash, no audio-thread lock. The snapshot index only changes on mouse-up.
*   Feedback patch after step 19: VCF SigOut to a test gain of 0.5 and back into SigIn is not possible with one SigIn. Use Ring as a cycle: A and B fed from Out through one delayed cable and one fan-out, or the unit-test cycle. The host check is: a user-made cycle does not halt the audio thread for one buffer of 64.

If the smoke test glitches, the rollback is the build-guide rollback for the step that introduced the glitch, not a new thread inside `processBlock`.

## Listening checks against public video

Do not commit the videos or the bounces.

| When | Source | Pass condition |
|---|---|---|
| After step 8 and again after step 20 | Gladén `MwJk36KnB04`, MS-50 sections at 0:51, 2:28, 4:20 | Low peak is duller at low cutoff. High peak rings and stays finite. Step 20: a louder saw is not a scaled copy of a quiet saw |
| After step 12 | Synth Party `C6oqCY0ArvM` after 0:51, as a reminder of Hz/V plus S-trig | Plugin Oct/V and Hz/V tests already encode the laws. The video is a sanity listen, not a pitch measurement |
| After step 11 | Alex Ball `3YFDDDXw6F8` near 8:43 | Optional. Confirms you are not listening to a Korg-35 story. Not a measurement |
| Never as a timbre reference | Dr. Kunz `_iJJYsWUYd0`, Perfect Circuit `UXW39LO-bxY` | Effects or a second synth are in the file |

Write mismatches into `docs/listening/step-20.md`. Change a stand-in only in a follow-up commit that names the id.
