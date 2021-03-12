#ifndef JAYD_FIRMWARE_MATRIXBIG_H
#define JAYD_FIRMWARE_MATRIXBIG_H


#include "MatrixPartition.h"

class MatrixBig : public MatrixPartition {
public:
	MatrixBig(LEDmatrixImpl* matrix);

protected:
	void push() override;
};


#endif //JAYD_FIRMWARE_MATRIXBIG_H
