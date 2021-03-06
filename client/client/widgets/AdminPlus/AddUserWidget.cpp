// Copyright 2021 Dmitriy Trifonov
#include <QFormLayout>
#include "AddUserWidget.h"

AddUserWidget::AddUserWidget(QWidget *parent) : QWidget(parent) {
    loginLabel = new QLabel("Логин:", this);
    passwordLabel = new QLabel("Пароль:", this);
    nameLabel = new QLabel("Имя:", this);
    surnameLabel = new QLabel("Фамилия:", this);
    roleLabel = new QLabel("Роль:", this);

    roleBox = new QComboBox(this);
    roleBox->addItems({"admin+", "admin", "teacher", "student"});

    login = new QLineEdit(this);
    password = new QLineEdit(this);
    name = new QLineEdit(this);
    surname = new QLineEdit(this);
    addUser = new QPushButton("Добавить пользователя", this);
    goBack = new QPushButton("Назад", this);

    QFormLayout *layout = new QFormLayout();
    layout->addRow(loginLabel, login);
    layout->addRow(passwordLabel, password);
    layout->addRow(nameLabel, name);
    layout->addRow(surnameLabel, surname);
    layout->addRow(roleLabel, roleBox);
    layout->addWidget(addUser);
    layout->addWidget(goBack);

    setLayout(layout);
}
