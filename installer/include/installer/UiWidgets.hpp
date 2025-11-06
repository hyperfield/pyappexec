#ifndef INSTALLER_UIWIDGETS_HPP
#define INSTALLER_UIWIDGETS_HPP

#include <QWidget>

class QLineEdit;
class QPushButton;

namespace installer {

struct BrowseRow
{
    QWidget* container{nullptr};
    QLineEdit* lineEdit{nullptr};
    QPushButton* browseButton{nullptr};
};

BrowseRow createBrowseRow(const QString& label, QWidget* parent);

} // namespace installer

#endif
