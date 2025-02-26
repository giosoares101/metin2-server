#ifndef __INC_DB_MATRIXCARD_H__
#define __INC_DB_MATRIXCARD_H__

extern bool MatrixCardCheck(const char * src, const char * answer, unsigned int rows, unsigned cols);
extern void MatrixCardRndCoordinate(unsigned int & rows, unsigned int & cols);

#define MATRIX_CARD_ROW(rows, i) ((rows >> ((4 - i - 1) * 8)) & 0x000000FF)
#define MATRIX_CARD_COL(cols, i) ((cols >> ((4 - i - 1) * 8)) & 0x000000FF)

#endif
