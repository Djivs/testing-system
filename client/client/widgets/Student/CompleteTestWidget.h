// Copyright 2021 Dmitriy Trifonov
#ifndef COMPLETETESTWIDGET_H
#define COMPLETETESTWIDGET_H

#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QList>
#include <QTextBrowser>
#include <QGroupBox>
#include <QVBoxLayout>
#include"../../../../lib/ImageViewDialog.h"
class CompleteTestWidget : public QDialog {
    Q_OBJECT

 public:
    explicit CompleteTestWidget(QList <QList <QString>> list, QWidget *parent = nullptr);

 public:
    QString percent;
    QString testname;

 private:
    QList <QRadioButton *> answerOptionButtons;
    QGroupBox *buttonsBox;
    QTextBrowser *taskText;
    QPushButton *next;
    QPushButton *previous;
    QPushButton *image;
    QPushButton *finish;
    QList <QList <QString>> testList;
    QVBoxLayout *generalLayout;

 private:
    int currentIndex;
    QList <QString> answers;
    QList <bool> isAnswersCorrect;

 signals:
    void finished();

 private:
    void setIndexTask();
    void askIfFinished();
};

#endif  // COMPLETETESTWIDGET_H
