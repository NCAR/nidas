#include "CalibrationPlot.h"

#include <iostream>

//#include "logx/Logging.h"

//LOGGING("CalibrationPlot");

CalibrationPlot::CalibrationPlot(QWidget* parent) : QWidget(parent)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    setupUi(parent);

    setupColors();
}

/* -------------------------------------------------------------------- */

void CalibrationPlot::dropColors()
{
    colors.clear();
}

/* -------------------------------------------------------------------- */

void CalibrationPlot::setupColors()
{
    colors.push_back( QColor(200,  0,  0) ); //  1 red
    colors.push_back( QColor(  0,200,200) ); //  7 skyblue
    colors.push_back( QColor(200,150,  0) ); //  2 orange
    colors.push_back( QColor(  0,150,200) ); //  8 lightblue
    colors.push_back( QColor(200,200,  0) ); //  3 yellow
    colors.push_back( QColor(  0,  0,200) ); //  9 blue
    colors.push_back( QColor(150,200,  0) ); //  4 yellowgreen
    colors.push_back( QColor(150,  0,200) ); // 10 purple
    colors.push_back( QColor(  0,200,  0) ); //  5 green
    colors.push_back( QColor(200,  0,200) ); // 11 fusha
    colors.push_back( QColor(  0,200,150) ); //  6 seagreen
    colors.push_back( QColor(200,  0,150) ); // 12 pink
/*
    colors.push_back( QColor(200,  0,  0) ); //  1 red
    colors.push_back( QColor(200,150,  0) ); //  2 orange
    colors.push_back( QColor(200,200,  0) ); //  3 yellow
    colors.push_back( QColor(150,200,  0) ); //  4 yellowgreen
    colors.push_back( QColor(  0,200,  0) ); //  5 green
    colors.push_back( QColor(  0,200,150) ); //  6 seagreen
    colors.push_back( QColor(  0,200,200) ); //  7 skyblue
    colors.push_back( QColor(  0,150,200) ); //  8 lightblue
    colors.push_back( QColor(  0,  0,200) ); //  9 blue
    colors.push_back( QColor(150,  0,200) ); // 10 purple
    colors.push_back( QColor(200,  0,200) ); // 11 fusha
    colors.push_back( QColor(200,  0,150) ); // 12 pink
*/
}
