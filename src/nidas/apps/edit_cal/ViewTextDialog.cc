#include "ViewTextDialog.h"

/* -------------------------------------------------------------------- */

ViewTextDialog::ViewTextDialog(QWidget *parent) : QDialog(parent)
{
    setupUi(this);
}

/* -------------------------------------------------------------------- */

void ViewTextDialog::setContents(const QString *text)
{
    textView->setFontFamily("Courier");
    textView->setPlainText(*text);
}
