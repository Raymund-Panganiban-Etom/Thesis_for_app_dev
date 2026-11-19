#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class CornClassifier {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        uint8_t votes[3] = { 0 };
                        // tree #1
                        if (x[0] <= 28.059999465942383) {
                            votes[1] += 1;
                        }

                        else {
                            if (x[1] <= 68.9000015258789) {
                                if (x[2] <= 4.75) {
                                    if (x[2] <= 2.0330000519752502) {
                                        votes[0] += 1;
                                    }

                                    else {
                                        votes[1] += 1;
                                    }
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }

                            else {
                                if (x[2] <= 3.4735000133514404) {
                                    votes[0] += 1;
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }
                        }

                        // tree #2
                        if (x[2] <= 4.75) {
                            if (x[2] <= 2.0290000438690186) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #3
                        if (x[1] <= 68.9000015258789) {
                            if (x[1] <= 66.9749984741211) {
                                if (x[1] <= 64.77499961853027) {
                                    if (x[0] <= 29.41999912261963) {
                                        votes[1] += 1;
                                    }

                                    else {
                                        if (x[2] <= 2.028000056743622) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }
                                }

                                else {
                                    if (x[2] <= 3.4635000228881836) {
                                        votes[0] += 1;
                                    }

                                    else {
                                        votes[2] += 1;
                                    }
                                }
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            if (x[2] <= 3.4735000133514404) {
                                votes[0] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #4
                        if (x[2] <= 4.74399995803833) {
                            if (x[1] <= 68.8849983215332) {
                                if (x[0] <= 29.505000114440918) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[0] <= 30.854999542236328) {
                                        votes[0] += 1;
                                    }

                                    else {
                                        if (x[1] <= 63.6200008392334) {
                                            votes[1] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }
                                }
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #5
                        if (x[2] <= 4.75) {
                            if (x[1] <= 68.9000015258789) {
                                if (x[0] <= 29.505000114440918) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[0] <= 31.27500057220459) {
                                        if (x[0] <= 30.854999542236328) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }

                                    else {
                                        votes[1] += 1;
                                    }
                                }
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #6
                        if (x[1] <= 68.9000015258789) {
                            if (x[1] <= 66.9749984741211) {
                                if (x[1] <= 64.77499961853027) {
                                    if (x[1] <= 62.375) {
                                        if (x[1] <= 62.114999771118164) {
                                            votes[1] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }

                                    else {
                                        if (x[2] <= 2.027500033378601) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }
                                }

                                else {
                                    if (x[1] <= 65.81499862670898) {
                                        if (x[0] <= 30.739999771118164) {
                                            votes[2] += 1;
                                        }

                                        else {
                                            votes[0] += 1;
                                        }
                                    }

                                    else {
                                        if (x[2] <= 2.7329999953508377) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }
                                }
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            if (x[1] <= 69.23500061035156) {
                                if (x[0] <= 28.234999656677246) {
                                    if (x[0] <= 28.22499942779541) {
                                        if (x[0] <= 28.195000648498535) {
                                            votes[2] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }

                                    else {
                                        if (x[1] <= 69.15499877929688) {
                                            votes[2] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }

                            else {
                                if (x[0] <= 28.25) {
                                    if (x[2] <= 3.4735000133514404) {
                                        votes[0] += 1;
                                    }

                                    else {
                                        votes[2] += 1;
                                    }
                                }

                                else {
                                    if (x[1] <= 69.31499862670898) {
                                        votes[2] += 1;
                                    }

                                    else {
                                        if (x[1] <= 69.3499984741211) {
                                            votes[2] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }
                                }
                            }
                        }

                        // tree #7
                        if (x[1] <= 68.9000015258789) {
                            if (x[2] <= 4.75) {
                                if (x[2] <= 2.0330000519752502) {
                                    votes[0] += 1;
                                }

                                else {
                                    votes[1] += 1;
                                }
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        else {
                            if (x[1] <= 69.35499954223633) {
                                if (x[2] <= 3.450500011444092) {
                                    votes[0] += 1;
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }

                            else {
                                if (x[1] <= 69.375) {
                                    if (x[1] <= 69.3650016784668) {
                                        if (x[2] <= 3.462000012397766) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }

                                    else {
                                        if (x[2] <= 3.431999981403351) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }
                                }

                                else {
                                    if (x[2] <= 3.1710000038146973) {
                                        votes[0] += 1;
                                    }

                                    else {
                                        votes[2] += 1;
                                    }
                                }
                            }
                        }

                        // tree #8
                        if (x[0] <= 28.045000076293945) {
                            votes[1] += 1;
                        }

                        else {
                            if (x[1] <= 68.9000015258789) {
                                if (x[2] <= 4.75) {
                                    if (x[1] <= 66.9749984741211) {
                                        if (x[0] <= 30.839999198913574) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }

                                    else {
                                        votes[1] += 1;
                                    }
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }

                            else {
                                if (x[2] <= 3.4735000133514404) {
                                    votes[0] += 1;
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }
                        }

                        // tree #9
                        if (x[2] <= 4.75) {
                            if (x[1] <= 68.9000015258789) {
                                if (x[0] <= 29.5) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[0] <= 31.630000114440918) {
                                        if (x[1] <= 64.64999961853027) {
                                            votes[1] += 1;
                                        }

                                        else {
                                            votes[0] += 1;
                                        }
                                    }

                                    else {
                                        votes[1] += 1;
                                    }
                                }
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #10
                        if (x[0] <= 28.059999465942383) {
                            votes[1] += 1;
                        }

                        else {
                            if (x[1] <= 68.9000015258789) {
                                if (x[0] <= 29.489999771118164) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[0] <= 30.854999542236328) {
                                        if (x[2] <= 3.4635000228881836) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }

                                    else {
                                        if (x[0] <= 31.27500057220459) {
                                            votes[1] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }
                                }
                            }

                            else {
                                if (x[0] <= 28.164999961853027) {
                                    votes[0] += 1;
                                }

                                else {
                                    if (x[0] <= 28.25) {
                                        if (x[0] <= 28.195000648498535) {
                                            votes[2] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }

                                    else {
                                        if (x[2] <= 2.743499994277954) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }
                                }
                            }
                        }

                        // tree #11
                        if (x[0] <= 28.059999465942383) {
                            votes[1] += 1;
                        }

                        else {
                            if (x[2] <= 4.738500118255615) {
                                if (x[0] <= 28.234999656677246) {
                                    if (x[2] <= 2.1449999809265137) {
                                        votes[0] += 1;
                                    }

                                    else {
                                        votes[1] += 1;
                                    }
                                }

                                else {
                                    if (x[2] <= 2.0330000519752502) {
                                        votes[0] += 1;
                                    }

                                    else {
                                        votes[1] += 1;
                                    }
                                }
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #12
                        if (x[2] <= 4.75) {
                            if (x[1] <= 68.91500091552734) {
                                if (x[0] <= 29.505000114440918) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[1] <= 64.68000221252441) {
                                        if (x[2] <= 2.0550000071525574) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }

                                    else {
                                        votes[0] += 1;
                                    }
                                }
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #13
                        if (x[2] <= 4.75) {
                            if (x[0] <= 28.059999465942383) {
                                votes[1] += 1;
                            }

                            else {
                                if (x[0] <= 28.22499942779541) {
                                    if (x[1] <= 68.93000030517578) {
                                        votes[1] += 1;
                                    }

                                    else {
                                        votes[0] += 1;
                                    }
                                }

                                else {
                                    if (x[1] <= 68.8849983215332) {
                                        if (x[1] <= 67.00500106811523) {
                                            votes[1] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }

                                    else {
                                        votes[0] += 1;
                                    }
                                }
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #14
                        if (x[2] <= 4.75) {
                            if (x[0] <= 28.05500030517578) {
                                votes[1] += 1;
                            }

                            else {
                                if (x[1] <= 68.9000015258789) {
                                    if (x[0] <= 29.505000114440918) {
                                        votes[1] += 1;
                                    }

                                    else {
                                        if (x[1] <= 64.64999961853027) {
                                            votes[1] += 1;
                                        }

                                        else {
                                            votes[0] += 1;
                                        }
                                    }
                                }

                                else {
                                    votes[0] += 1;
                                }
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #15
                        if (x[1] <= 68.9000015258789) {
                            if (x[1] <= 66.9749984741211) {
                                if (x[0] <= 29.309999465942383) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[1] <= 64.64999961853027) {
                                        if (x[1] <= 62.42499923706055) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[1] += 1;
                                        }
                                    }

                                    else {
                                        if (x[2] <= 3.4524999856948853) {
                                            votes[0] += 1;
                                        }

                                        else {
                                            votes[2] += 1;
                                        }
                                    }
                                }
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            if (x[2] <= 3.450500011444092) {
                                votes[0] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // return argmax of votes
                        uint8_t classIdx = 0;
                        float maxVotes = votes[0];

                        for (uint8_t i = 1; i < 3; i++) {
                            if (votes[i] > maxVotes) {
                                classIdx = i;
                                maxVotes = votes[i];
                            }
                        }

                        return classIdx;
                    }

                protected:
                };
            }
        }
    }