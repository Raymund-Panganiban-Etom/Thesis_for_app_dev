#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class BananaClassifier {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        uint8_t votes[3] = { 0 };
                        // tree #1
                        if (x[0] <= 36.010000228881836) {
                            if (x[1] <= 70.9900016784668) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #2
                        if (x[2] <= 7.849999904632568) {
                            if (x[2] <= 3.850000023841858) {
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
                        if (x[1] <= 63.5) {
                            votes[2] += 1;
                        }

                        else {
                            if (x[1] <= 71.03499984741211) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        // tree #4
                        if (x[2] <= 7.849999904632568) {
                            if (x[1] <= 70.97999954223633) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #5
                        if (x[2] <= 7.849999904632568) {
                            if (x[1] <= 70.9900016784668) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #6
                        if (x[1] <= 63.5) {
                            votes[2] += 1;
                        }

                        else {
                            if (x[1] <= 70.96500015258789) {
                                votes[1] += 1;
                            }

                            else {
                                if (x[1] <= 71.03499984741211) {
                                    if (x[0] <= 31.359999656677246) {
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

                        // tree #7
                        if (x[1] <= 63.51499938964844) {
                            votes[2] += 1;
                        }

                        else {
                            if (x[2] <= 3.850000023841858) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #8
                        if (x[0] <= 36.010000228881836) {
                            if (x[1] <= 71.02000045776367) {
                                if (x[2] <= 2.0) {
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

                        else {
                            votes[2] += 1;
                        }

                        // tree #9
                        if (x[2] <= 7.849999904632568) {
                            if (x[1] <= 70.9900016784668) {
                                votes[1] += 1;
                            }

                            else {
                                if (x[0] <= 31.69499969482422) {
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

                        // tree #10
                        if (x[0] <= 36.010000228881836) {
                            if (x[1] <= 70.9900016784668) {
                                if (x[0] <= 35.9950008392334) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[2] <= 6.671000003814697) {
                                        votes[1] += 1;
                                    }

                                    else {
                                        votes[2] += 1;
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

                        // tree #11
                        if (x[0] <= 36.010000228881836) {
                            if (x[2] <= 3.850000023841858) {
                                votes[0] += 1;
                            }

                            else {
                                if (x[0] <= 35.9950008392334) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[2] <= 6.671000003814697) {
                                        votes[1] += 1;
                                    }

                                    else {
                                        votes[2] += 1;
                                    }
                                }
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #12
                        if (x[2] <= 7.849999904632568) {
                            if (x[1] <= 71.02000045776367) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #13
                        if (x[2] <= 7.849999904632568) {
                            if (x[0] <= 31.010000228881836) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #14
                        if (x[2] <= 7.849999904632568) {
                            if (x[0] <= 31.010000228881836) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #15
                        if (x[1] <= 63.5) {
                            votes[2] += 1;
                        }

                        else {
                            if (x[1] <= 70.9900016784668) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
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
