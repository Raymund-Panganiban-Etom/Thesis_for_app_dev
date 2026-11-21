#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class CalamansiClassifier {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        uint8_t votes[3] = { 0 };
                        // tree #1
                        if (x[0] <= 31.010000228881836) {
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

                        // tree #2
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[2] <= 4.75) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #3
                        if (x[1] <= 71.04999923706055) {
                            if (x[1] <= 63.48500061035156) {
                                votes[2] += 1;
                            }

                            else {
                                if (x[1] <= 70.96500015258789) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[0] <= 31.899999618530273) {
                                        votes[0] += 1;
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

                        // tree #4
                        if (x[2] <= 2.0385000109672546) {
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

                        // tree #5
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.52000045776367) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #6
                        if (x[1] <= 70.9749984741211) {
                            if (x[1] <= 63.5) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            if (x[1] <= 71.04999923706055) {
                                if (x[0] <= 31.899999618530273) {
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

                        // tree #7
                        if (x[1] <= 71.04999923706055) {
                            if (x[2] <= 4.75) {
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
                            if (x[1] <= 68.38000106811523) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            if (x[2] <= 4.75) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
                            }
                        }

                        // tree #9
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.4950008392334) {
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
                            if (x[1] <= 63.4950008392334) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #11
                        if (x[0] <= 31.010000228881836) {
                            if (x[2] <= 2.943999946117401) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            if (x[0] <= 36.010000228881836) {
                                if (x[2] <= 4.75) {
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
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[1] <= 63.5099983215332) {
                                votes[2] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #13
                        if (x[2] <= 2.0385000109672546) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[0] <= 36.010000228881836) {
                                if (x[0] <= 35.9950008392334) {
                                    votes[1] += 1;
                                }

                                else {
                                    if (x[1] <= 63.650001525878906) {
                                        votes[2] += 1;
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

                        // tree #14
                        if (x[2] <= 2.0330000519752502) {
                            votes[0] += 1;
                        }

                        else {
                            if (x[0] <= 36.010000228881836) {
                                if (x[1] <= 63.43000030517578) {
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
                        if (x[1] <= 71.04999923706055) {
                            if (x[1] <= 63.4950008392334) {
                                votes[2] += 1;
                            }

                            else {
                                if (x[0] <= 31.010000228881836) {
                                    if (x[1] <= 68.38000106811523) {
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
