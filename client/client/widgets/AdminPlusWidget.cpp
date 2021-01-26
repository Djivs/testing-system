#include "AdminPlusWidget.h"

AdminPlusWidget::AdminPlusWidget(QWidget *parent) : QWidget(parent)
{
    addGroup = new QPushButton("&Add group", this);
    addToGroup = new QPushButton("&Add to group", this);
    appointGroup = new QPushButton("&Appoint group", this);
    addUser = new QPushButton("&Add user", this);
    layout = new QVBoxLayout();
    layout->addWidget(addGroup);
    layout->addWidget(addToGroup);
    layout->addWidget(appointGroup);
    layout->addWidget(addUser);

    this->setLayout(layout);
}
