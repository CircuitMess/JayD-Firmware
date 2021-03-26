#ifndef JAYD_FIRMWARE_MATRIXR_H
#define JAYD_FIRMWARE_MATRIXR_H


#include "MatrixPartition.h"

class MatrixR : public MatrixPartition {
public:
	MatrixR(LEDmatrixImpl* matrix);

	void push() override;
protected:
};


#endif //JAYD_FIRMWARE_MATRIXR_H
