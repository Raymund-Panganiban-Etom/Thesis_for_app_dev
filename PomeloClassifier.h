#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class PomeloClassifier {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        uint8_t votes[3] = { 0 };
                        // tree #1
                        if (x[0] <= 31.010000228881836) {
                            if (x[1] <= 68.96500015258789) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            if (x[2] <= 4.733000040054321) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #2
                        if (x[2] <= 2.0550000071525574) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[2] <= 4.733000040054321) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #3
                        if (x[1] <= 70.97999954223633) {
                            if (x[1] <= 63.58000183105469) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            votes[0] += 1;
                        }

                        // tree #4
                        if (x[2] <= 2.0550000071525574) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.58000183105469) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #5
                        if (x[2] <= 2.0550000071525574) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.5) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #6
                        if (x[1] <= 70.97999954223633) {
                            if (x[1] <= 63.5) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            votes[0] += 1;
                        }

                        // tree #7
                        if (x[1] <= 70.97999954223633) {
                            if (x[2] <= 4.733000040054321) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        else {
                            votes[0] += 1;
                        }

                        // tree #8
                        if (x[0] <= 31.010000228881836) {
                            if (x[1] <= 68.91500091552734) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            if (x[2] <= 4.733000040054321) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #9
                        if (x[2] <= 2.087999999523163) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.5) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #10
                        if (x[0] <= 31.010000228881836) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.60499954223633) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #11
                        if (x[0] <= 30.989999771118164) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[2] <= 4.733000040054321) {
                                if (x[0] <= 31.010000228881836) {
                                    if (x[2] <= 2.8920000791549683) {
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

                        // tree #12
                        if (x[2] <= 2.0550000071525574) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.58000183105469) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #13
                        if (x[2] <= 2.0550000071525574) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[0] <= 36.02499961853027) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #14
                        if (x[2] <= 2.0550000071525574) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[0] <= 36.02000045776367) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #15
                        if (x[1] <= 70.97999954223633) {
                            if (x[1] <= 63.4950008392334) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            votes[0] += 1;
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
