#include "installer/UiWidgets.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QObject>

namespace installer {

BrowseRow createBrowseRow(const QString& label, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* textLabel = new QLabel(label, container);
    auto* edit = new QLineEdit(container);
    auto* button = new QPushButton(QObject::tr("Browse"), container);

    layout->addWidget(textLabel);
    layout->addWidget(edit, 1);
    layout->addWidget(button);

    BrowseRow row;
    row.container = container;
    row.lineEdit = edit;
    row.browseButton = button;
    return row;
}

} // namespace installer
