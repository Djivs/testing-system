// Copyright 2021 Dmitriy Trifonov
#ifndef ALLRESULTSWIDGET_H
#define ALLRESULTSWIDGET_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

class AllResultsWidget : public QWidget {
    Q_OBJECT

 public:
    explicit AllResultsWidget(QWidget *parent = nullptr, QList <QList <QString>> l = {});

 public:
    QPushButton *goBack;
    QList <QList<QString>> list;

 private:
    void showAllResultsSort();
    void editAllResults();

 private:
    QPushButton *sort;
    QPushButton *save;
    QTableView *table;
    QStandardItemModel *model;
    QLabel *sortNameLabel;
    QLabel *sortSurnameLabel;
    QLabel *sortSubjectLabel;
    QLabel *sortTestLabel;
    QLineEdit *sortName;
    QLineEdit *sortSurname;
    QLineEdit *sortSubject;
    QLineEdit *sortTest;
};

#endif  // ALLRESULTSWIDGET_H
