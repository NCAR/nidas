HEADERS       = CalibrationWizard.h
SOURCES       = CalibrationWizard.cpp \
                main.cpp
RESOURCES     = CalibrationWizard.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/dialogs/CalibrationWizard
sources.files = $$SOURCES $$HEADERS $$FORMS $$RESOURCES *.pro images
sources.path = $$[QT_INSTALL_EXAMPLES]/dialogs/CalibrationWizard
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
