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
                        if (x[0] <= 35.989999771118164) {
                            if (x[1] <= 71.02000045776367) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        else {
                            if (x[2] <= 4.355499982833862) {
                                votes[1] += 1;
                            }

                            else {
                                votes[2] += 1;
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
                        if (x[1] <= 63.5) {
                            votes[2] += 1;
                        }

                        else {
                            if (x[1] <= 71.02000045776367) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        // tree #4
                        if (x[2] <= 4.74399995803833) {
                            if (x[1] <= 71.01499938964844) {
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
                        if (x[2] <= 4.75) {
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

                        // tree #6
                        if (x[1] <= 63.5) {
                            votes[2] += 1;
                        }

                        else {
                            if (x[1] <= 71.06499862670898) {
                                votes[1] += 1;
                            }

                            else {
                                votes[0] += 1;
                            }
                        }

                        // tree #7
                        if (x[1] <= 63.5) {
                            votes[2] += 1;
                        }

                        else {
                            if (x[2] <= 2.0330000519752502) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        // tree #8
                        if (x[0] <= 36.010000228881836) {
                            if (x[1] <= 71.05500030517578) {
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
                        }

                        else {
                            votes[2] += 1;
                        }

                        // tree #9
                        if (x[2] <= 4.75) {
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

                        // tree #10
                        if (x[0] <= 36.010000228881836) {
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

                        // tree #11
                        if (x[0] <= 35.9950008392334) {
                            if (x[2] <= 2.0330000519752502) {
                                votes[0] += 1;
                            }

                            else {
                                votes[1] += 1;
                            }
                        }

                        else {
                            if (x[0] <= 36.02499961853027) {
                                if (x[2] <= 4.472000002861023) {
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
                        if (x[2] <= 4.75) {
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
                        if (x[2] <= 4.75) {
                            if (x[0] <= 31.0) {
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
                        if (x[2] <= 4.75) {
                            if (x[0] <= 30.989999771118164) {
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
                            if (x[1] <= 71.02000045776367) {
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
