#ifndef KNOB_H
#define KNOB_H

class Knob {
private:
    int rotation;
    int minimum, maximum;
    bool A, B;
    bool rotPlusOnePrev, rotMinOnePrev;

public:
    Knob(int minimum, int max);

    int getRotation();

    void updateRotation(bool ANew, bool BNew);
};

#endif