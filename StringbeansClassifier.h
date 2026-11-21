#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class StringbeansClassifier {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        uint8_t votes[3] = { 0 };
                        // tree #1
                        if (x[0] <= 31.039999961853027) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.56999969482422) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #2
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[2] <= 4.649499893188477) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #3
                        if (x[1] <= 70.96500015258789) {
                            if (x[1] <= 63.56999969482422) {
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
                        if (x[2] <= 2.0330000519752502) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.6299991607666) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #5
                        if (x[2] <= 2.0290000438690186) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.56999969482422) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #6
                        if (x[1] <= 70.97000122070312) {
                            if (x[1] <= 63.56999969482422) {
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
                        if (x[1] <= 70.97000122070312) {
                            if (x[2] <= 4.649499893188477) {
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
                        if (x[0] <= 30.984999656677246) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.5) {
                                votes[2] += 1;
                            }

                            else {
                                if (x[2] <= 1.746500015258789) {
                                    votes[0] += 1;
                                }

                                else {
                                    votes[1] += 1;
                                }
                            }
                        }

                        // tree #9
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.56999969482422) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #10
                        if (x[0] <= 30.984999656677246) {
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

                        // tree #11
                        if (x[0] <= 31.024999618530273) {
                            if (x[2] <= 2.1940000653266907) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            if (x[0] <= 36.010000228881836) {
                                if (x[2] <= 4.649499893188477) {
                                    votes[1] += 1;
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #12
                        if (x[2] <= 2.07150000333786) {
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

                        // tree #13
                        if (x[2] <= 2.0330000519752502) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[0] <= 35.95000076293945) {
                                votes[1] += 1;
                            }

                            else {
                                if (x[0] <= 36.010000228881836) {
                                    if (x[1] <= 64.33500099182129) {
                                        votes[2] += 1;
                                    }

                                    else {
                                        votes[1] += 1;
                                    }
                                }

                                else {
                                    votes[2] += 1;
                                }
                            }
                        }

                        // tree #14
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[0] <= 36.010000228881836) {
                                if (x[1] <= 63.44999885559082) {
                                    votes[2] += 1;
                                }

                                else {
                                    votes[1] += 1;
                                }
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #15
                        if (x[1] <= 70.96500015258789) {
                            if (x[1] <= 63.625) {
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
