#ifndef JAYD_FIRMWARE_MATRIXL_H
#define JAYD_FIRMWARE_MATRIXL_H

#include "MatrixPartition.h"

class MatrixL : public MatrixPartition {
public:
	MatrixL(LEDmatrixImpl* matrix);

	void push() override;
protected:
};


#endif //JAYD_FIRMWARE_MATRIXL_H
