#include "authorization.h"

Authorization::Authorization(QWidget *parent) : QWidget(parent)
{
    loginLabel = new QLabel(this);
    loginLabel->setText("Login:");
    loginLabel->resize(100, 25);

    passwordLabel = new QLabel(this);
    passwordLabel->setText("Password:");
    passwordLabel->resize(100, 20);

    login = new QLineEdit(this);
    login->resize(200, 20);
    password = new QLineEdit(this);
    password->resize(200, 20);

    authorize = new QPushButton("&Log in", this);
    connect(authorize, &QPushButton::clicked, this, [this] {emit logInButtonClicked();});

    QHBoxLayout *l1 = new QHBoxLayout();
    l1->addWidget(loginLabel);
    l1->addWidget(login);

    QHBoxLayout *l2 = new QHBoxLayout();
    l2->addWidget(passwordLabel);
    l2->addWidget(password);

    layout = new QVBoxLayout();
    layout->addLayout(l1);
    layout->addLayout(l2);
    layout->addWidget(authorize);

    this->setLayout(layout);
}
