#include "knob.h"

Knob::Knob(int minimum, int maximum) {
    Knob::minimum = minimum;
    Knob::maximum = maximum;
    Knob::A = false;
    Knob::B = false;
    Knob::rotPlusOnePrev = false;
    Knob::rotMinOnePrev = false;
    Knob::rotation = 0;
}

int Knob::getRotation() { 
    return Knob::rotation; 
};

void Knob::updateRotation(bool ANew, bool BNew) {
		bool rotPlusOneNew = 	(!B && !A && !BNew && ANew) ||
								(!B && A && BNew && ANew) ||
								(B && !A && !BNew && !ANew) ||
								(B && A && BNew && !ANew);

		bool rotMinOneNew = 	(!B && !A && BNew && !ANew) ||
								(!B && A && !BNew && !ANew) ||
								(B && !A && BNew && ANew) ||
								(B && A && !BNew && ANew); 

		bool impossibleState =	(!B && !A && BNew && ANew) ||
								(!B && A && BNew && !ANew) ||
								(B && !A && !BNew && ANew) ||
								(B && A && !BNew && !ANew); 
	
		if (rotPlusOneNew || (impossibleState && rotPlusOnePrev)) rotation += 1;
		if (rotMinOneNew ||  (impossibleState && rotMinOnePrev)) rotation -= 1;
		if (rotation < minimum) rotation = minimum;
		if (rotation > maximum) rotation = maximum;

		A = ANew;
		B = BNew;
		if (!impossibleState) {
			rotPlusOnePrev = rotPlusOneNew;
			rotMinOnePrev = rotMinOneNew;
		}
}