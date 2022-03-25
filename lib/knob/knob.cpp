#include <knob>

Knob::Knob(int minimum, int maximum, int initialRotation) {
	Knob::minimum = minimum;
	Knob::maximum = maximum;
	Knob::A = false;
	Knob::B = false;
	Knob::previousRotation = NONE;
	Knob::rotation = initialRotation;
	Knob::rotationInternal = initialRotation;
}

int Knob::getRotation() {
	return Knob::rotation;
};

void Knob::updateRotation(bool ANew, bool BNew) {
	if (A == ANew && B == BNew)
		return; // No change, do not update values

	bool fullstep = (BNew && ANew) || (!BNew && !ANew);

	bool cwRot = (!B && !A && !BNew && ANew) ||
				 (!B && A && BNew && ANew) ||
				 (B && !A && !BNew && !ANew) ||
				 (B && A && BNew && !ANew);

	bool acwRot = (!B && !A && BNew && !ANew) ||
				  (!B && A && !BNew && !ANew) ||
				  (B && !A && BNew && ANew) ||
				  (B && A && !BNew && ANew);

	bool impossible = (!B && !A && BNew && ANew) ||
					  (!B && A && BNew && !ANew) ||
					  (B && !A && !BNew && ANew) ||
					  (B && A && !BNew && !ANew);

	if (cwRot) {
		if (previousRotation == CW && fullstep) {
			rotationInternal++;
		} else if (previousRotation == ACW) {
			previousRotation = NONE;
		} else {
			previousRotation = CW;
		}
	} else if (acwRot) {
		if (previousRotation == ACW && fullstep) {
			rotationInternal--;
		} else if (previousRotation == CW) {
			previousRotation = NONE;
		} else {
			previousRotation = ACW;
		}
	} else if (impossible) {
		if (fullstep) {
			switch (previousRotation) {
				case NONE: // Step skipped and no previous direction
					break;
				case CW:
					rotationInternal++;
					break;
				case ACW:
					rotationInternal--;
					break;
			}
		}
	}
	A = ANew;
	B = BNew;
	if (rotationInternal < minimum)
		rotationInternal = minimum;
	if (rotationInternal > maximum)
		rotationInternal = maximum;
	rotation = rotationInternal;
}

void Knob::changeLimitsVolume(int newMinimum, int newMaximum) {
    if(newMaximum>maximum){
        rotation = rotation<<1;
    }else if(newMaximum<maximum){
        rotation = rotation>>1;
    }else{}
    minimum = newMinimum;
    maximum = newMaximum;
};