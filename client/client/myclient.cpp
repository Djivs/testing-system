// Copyright 2021 Dmitriy Trifonov
#include <QFileInfo>
#include <QBuffer>
#include <QTextCodec>
#include <QNetworkReply>
#include <QEventLoop>
#include "myclient.h"

MyClient::MyClient(const QString& strHost, int nPort, QWidget *parent)
    : QMainWindow(parent) {
    m_nNextBlockSize = 0;
    m_pTcpSocket = new QTcpSocket(this);
    m_pTcpSocket->connectToHost(strHost, nPort);


    connect(m_pTcpSocket, &QTcpSocket::connected,
            this, [this] {showMsg("Received the connected() signal");});
    connect(m_pTcpSocket, SIGNAL(readyRead()), SLOT(slotReadyRead()));
    connect(m_pTcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this,         SLOT(slotError(QAbstractSocket::SocketError)));

    m_manager = new QNetworkAccessManager(this);
    connect(m_manager, &QNetworkAccessManager::finished, this, [] {qDebug() << "Finished";});

    setMinimumSize(WINW, WINH);
    setWindowTitle("Тестирующая система");

    setAuthorizationWindow();
}

MyClient::~MyClient() {
}

void MyClient::setAuthorizationWindow() {
    aw = new AuthorizationWidget(this);
    connect(aw->authorize, &QPushButton::clicked, this, [this] {
        slotSendToServer("{cmd='authorize';login='" + aw->getLogin() + "';"
                         "pass='" + aw->getPassword() + "';}");
    });
    setCentralWidget(aw);
}

void MyClient::slotReadyRead() {
    QDataStream in(m_pTcpSocket);
    for (;;) {
        if (!m_nNextBlockSize) {
            if (m_pTcpSocket->bytesAvailable() < static_cast<quint16>(sizeof(quint16))) {
                break;
            }
            in >> m_nNextBlockSize;
        }

        if (m_pTcpSocket->bytesAvailable() < m_nNextBlockSize) {
            break;
        }
        QTime time;
        QString str;
        in >> time >> str;
        solveMsg(str);
        m_nNextBlockSize = 0;
    }
}

void MyClient::slotError(QAbstractSocket::SocketError err) {
    QString strError =
        "Error: " + (err == QAbstractSocket::HostNotFoundError ?
                     "The host was not found." :
                     err == QAbstractSocket::RemoteHostClosedError ?
                     "The remote host is closed." :
                     err == QAbstractSocket::ConnectionRefusedError ?
                     "The connection was refused." :
                     QString(m_pTcpSocket->errorString()));
    showError(strError);
}

void MyClient::slotSendToServer(QString msg) {
    QByteArray  arrBlock;
    QDataStream out(&arrBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_DefaultCompiledVersion);
    out << quint16(0) << QTime::currentTime() << QString(msg);

    out.device()->seek(0);
    out << quint16(arrBlock.size() - sizeof(quint16));
    m_pTcpSocket->write(arrBlock);
}

void MyClient::solveMsg(QString msg) {
    if (!StringOperator::validatePackage(msg)) {
        return;
    }
    QString cmd = StringOperator::cutArg(msg, "cmd");
    if (cmd == "authorize") {
        if (StringOperator::cutArg(msg, "status") == "0") {
            QString role = StringOperator::cutArg(msg, "role");
            showMsg("Успешный вход!\nРоль: " + role);
            id = StringOperator::cutArg(msg, "id").toInt();
            delete aw;
            if (role == "admin+")
                setAdminPlusWindow();
            else if (role == "admin")
                setAdminWindow();
            else if (role == "teacher")
                setTeacherWindow();
            else if (role == "student")
                setStudentWindow();
        } else {
           showError("Неправильный логин/пароль :(");
        }
    } else if (cmd == "add group") {
        QString r = StringOperator::cutArg(msg, "status");
        if (r == "0")
            showMsgBox("Группа успешно добавлена");
        else
            showError("This group already exists!");
    } else if (cmd == "add to group") {
        int r = StringOperator::cutArg(msg, "status").toInt();
        switch (r) {
        case 0:
            showMsg("Добавление в группу прошло успешно");
            break;
        case 1:
            showError("Группы с введённым именем не существует");
            break;
        case 2:
            showError("Ученика с введённым именем не существует");
            break;
        case 3:
            showError("Уже добавлен");
            break;
        }
    } else if (cmd == "appoint") {
        int r = StringOperator::cutArg(msg, "status").toInt();
        switch (r) {
        case 0:
            showMsg("Успешно назначено");
            break;
        case 1:
            showError("Введённые имя и фамилия не принадлежат ни одному учителю");
            break;
        case 2:
            showError("Группы с введённым именем не существует\nИли\nЭта группа уже была назначена");
            break;
        }
    } else if (cmd == "add user") {
        int r = StringOperator::cutArg(msg, "status").toInt();
        switch (r) {
        case 0:
            showMsg("Пользователь успешно добавлен");
            break;
        case 1:
            showError("Этот пользователь уже добавлен");
            break;
        }
    } else if (cmd == "view all results") {
        if (StringOperator::cutArg(msg, "status") == "sended") {
            if (allResultsList.isEmpty()) {
                showError("Результатов ещё нет");
                return;
            }
            delete adminW;
            arw = new AllResultsWidget(this, allResultsList);
            connect(arw->goBack, &QPushButton::clicked, this, [this] {delete arw; setAdminWindow();});
            setCentralWidget(arw);
            return;
        } else if (StringOperator::cutArg(msg, "status") == "started") {
            allResultsList.clear();
            return;
        }
        allResultsList.push_back({
        StringOperator::cutArg(msg, "testname"), StringOperator::cutArg(msg, "subject"),
        StringOperator::cutArg(msg, "studentsname"), StringOperator::cutArg(msg, "studentssurname"),
        StringOperator::cutArg(msg, "percent")});
    } else if (cmd == "view all groups") {
        if (StringOperator::cutArg(msg, "status") == "sended") {
            allGroupsW = new AllGroupsWidget(this, allGroupsList);
            state = VIEWALLGROUPS;
            for (int i = 0; i < allGroupsW->buttonsList.size(); ++i) {
                connect(allGroupsW->buttonsList[i], &QPushButton::clicked, this,
                    [this, i] () {slotSendToServer("{cmd='view group students';groupname='" + allGroupsW->buttonsList[i]->accessibleName() + "';}"); });
            }
            connect(allGroupsW->goBack, &QPushButton::clicked, this, [this] {
               delete allGroupsW;
               groupStudents.clear(); groupTeachers.clear();
               allGroupsList.clear();
               setAdminWindow();});
            setCentralWidget(allGroupsW);
            return;
        }
        allGroupsList.push_back({StringOperator::cutArg(msg, "name"), StringOperator::cutArg(msg, "teachername")});
    } else if (cmd == "view group students") {
        QString status = StringOperator::cutArg(msg, "status");
        if (status == "sended") {
            if (state != VIEWALLGROUPS)
                teacherGroupsW->showGroupStudents(groupStudents);
            else
                allGroupsW->showGroupStudents(groupStudents);
            groupStudents.clear();
        } else {
            groupStudents.push_back({StringOperator::cutArg(msg, "name"), StringOperator::cutArg(msg, "surname")});
        }
    } else if (cmd == "view all planned tests") {
        QString status = StringOperator::cutArg(msg, "status");
        if (status == "sended") {
            if (allPlannedTestsList.isEmpty()) {
                showError("Тестов ещё нет");
                return;
            }
            if (state == APPOINTTEST) {
                delete teacherW;
                QStringList testList;
                for (auto &i : allPlannedTestsList)
                    testList.push_back(i[2]);
                appointTestW = new AppointTestWidget(this, teacherGroups, testList);
                connect(appointTestW->goBack, &QPushButton::clicked, this, [this] {delete appointTestW; setTeacherWindow();});
                connect(appointTestW->submit, &QPushButton::clicked, this, [this] {
                    slotSendToServer("{cmd='appoint test';testname='" + appointTestW->getTest() + "';groupname='" + appointTestW->getGroup() + "';}");
                showMsg("Отправлено!"); });
                setCentralWidget(appointTestW);
            } else {
                atw = new AllTestsWidget(this, allPlannedTestsList);
                connect(atw->goBack, &QPushButton::clicked, this,
                        [this] {delete atw; setAdminWindow();});
                for (int i = 0; i < atw->buttons.size(); ++i) {
                    connect(atw->buttons[i], &QPushButton::clicked, this, [this, i]
                    {slotSendToServer("{cmd='view test tasks';testname='" + atw->buttons[i]->accessibleName() + "';}");});
                }
                setCentralWidget(atw);
            }
        } else if (status == "started") {
            allPlannedTestsList.clear();
        } else {
            allPlannedTestsList.push_back({StringOperator::cutArg(msg, "teachername"), StringOperator::cutArg(msg, "teachersurname"), StringOperator::cutArg(msg, "testname"),
                                          StringOperator::cutArg(msg, "subject"), StringOperator::cutArg(msg, "date")});
        }
    } else if (cmd == "view test tasks") {
        QString status = StringOperator::cutArg(msg, "status");
        if (status == "sended") {
            for (int i = 0; i < allPlannedTestsTaskList.size(); ++i) {
                QEventLoop loop;
                QNetworkAccessManager mgr;
                connect(&mgr, &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
                QUrl url("ftp://127.0.0.1/../../home/dmitriy/FTPServer/img/" + allPlannedTestsTaskList[i][5] + ".jpg");
                url.setUserName("dmitriy");
                url.setPassword("123");
                url.setPort(21);
                QNetworkReply *reply = mgr.get(QNetworkRequest(url));
                loop.exec();
                QFile img(allPlannedTestsTaskList[i][5] + ".jpg");
                img.open(QIODevice::WriteOnly);
                img.write(reply->readAll());
                qDebug() << allPlannedTestsTaskList[i][5];
            }
           if (state == COMPLETETEST) {
               delete studentTestsW;
               completeTestW = new CompleteTestWidget(allPlannedTestsTaskList, this);
               connect(completeTestW, &CompleteTestWidget::finished, this, [this]{
                   slotSendToServer("{cmd='add result';studentid='" + QString::number(id) + "';testname='" +
                   completeTestW->testname + "';completionpercent='" + completeTestW->percent + "';}");
                   delete completeTestW;
                   setStudentWindow();
               });
               setCentralWidget(completeTestW);
           } else {
              atw->showTestTasks(allPlannedTestsTaskList);
           }
        } else if (status == "started") {
            allPlannedTestsTaskList.clear();
        } else {
            allPlannedTestsTaskList.push_back({StringOperator::cutArg(msg, "testname"), StringOperator::cutArg(msg, "taskname"), StringOperator::cutArg(msg, "answeroptions"),
                                              StringOperator::cutArg(msg, "answertext"), StringOperator::cutArg(msg, "theme"), StringOperator::cutArg(msg, "imageid")});
        }
    } else if (cmd == "add task") {
        QString status = StringOperator::cutArg(msg, "status");
        if (status == "sended")
        {
            QString image = addTaskW->getImageFileName();
            if (image != ":NONE:") {
//                QFile *m_file = new QFile(image);
//                m_file->open(QIODevice::ReadOnly);
//                QFileInfo fileInfo(* m_file);
//                QUrl url("ftp://127.0.0.1/../../home/dmitriy/FTPServer/img" + fileInfo.fileName());
//                url.setUserName("dmitriy");
//                url.setPassword("123");
//                url.setPort(21);
//                QNetworkReply *reply = m_manager->put(QNetworkRequest(url), m_file);
//                connect(reply, &QNetworkReply::uploadProgress, this, [] (qint64 bytesSent, qint64 bytesTotal ){qDebug() << bytesSent << bytesTotal;});
                QString req = "{cmd='add task image';taskid='" + StringOperator::cutArg(msg, "taskid") + "';sender='" + QString::number(id) + "';}";
                slotSendToServer(req);
            }
            showMsg("Задание добавлено успешно");
        }
    } else if (cmd == "add task image") {
        QString name = StringOperator::cutArg(msg, "imageid");
        QFile *m_file = new QFile(addTaskW->getImageFileName());
        m_file->open(QIODevice::ReadOnly);
        QFileInfo fileInfo(* m_file);
        QUrl url("ftp://127.0.0.1/../../home/dmitriy/FTPServer/img/" + name + "." + fileInfo.completeSuffix());
        url.setUserName("dmitriy");
        url.setPassword("123");
        url.setPort(21);
        QNetworkReply *reply = m_manager->put(QNetworkRequest(url), m_file);
        connect(reply, &QNetworkReply::uploadProgress, this, [] (qint64 bytesSent, qint64 bytesTotal ){qDebug() << bytesSent << bytesTotal;});
    } else if (cmd == "add test") {
        showMsg("добавлено!");
    } else if (cmd == "get all tasks") {
        if (StringOperator::cutArg(msg, "status") == "sending") {
            allTasksList.push_back({StringOperator::cutArg(msg, "taskid"), StringOperator::cutArg(msg, "subject"), StringOperator::cutArg(msg, "tasktext"), StringOperator::cutArg(msg, "answeroptions")
                                   , StringOperator::cutArg(msg, "answertext"), StringOperator::cutArg(msg, "theme"), StringOperator::cutArg(msg, "teacherid")});
        } else {
            addTestW->setUserID(id);
            addTestW->setManualTaskList(allTasksList);
            addTestW->setManual();
        }
    } else if (cmd == "add separated test") {
        QString testId = StringOperator::cutArg(msg, "testid");
        QList <QString> list = addTestW->getPickedTasks();
        for (auto &i : list)
            slotSendToServer("{cmd='appoint task to test';taskid='" + i + "';testid='" + testId + "';}");
        showMsg("Отправлено!");
    } else if (cmd == "get teacher groups") {
        if (StringOperator::cutArg(msg, "status") == "sending") {
            teacherGroups.push_back(StringOperator::cutArg(msg, "groupname"));
        } else {
            if (state == APPOINTTEST) {
                slotSendToServer("{cmd='view all planned tests';}");
                return;
            }
            teacherGroupsW = new TeacherGroupsWidget(this, teacherGroups);
            connect(teacherGroupsW->goBack, &QPushButton::clicked, this, [this] {delete teacherGroupsW; setTeacherWindow();});
            for (int i = 0; i < teacherGroupsW->tableButtons.size(); ++i)
                connect(teacherGroupsW->tableButtons[i], &QPushButton::clicked, this, [this, i] {slotSendToServer("{cmd='view group students';groupname='" +
                                                                                                                  teacherGroupsW->tableButtons[i]->accessibleName() + "';}");});
            setCentralWidget(teacherGroupsW);
            teacherGroups.clear();
        }
    } else if (cmd == "get teacher results") {
        if (StringOperator::cutArg(msg, "status") == "sending") {
            QList <QString> params = {"studentsname", "studentssurname", "studentsgroup", "testname", "percent", "subject", "date"};
            QList <QString> buf;
            for (int i = 0; i < params.size(); ++i)
                buf.push_back(StringOperator::cutArg(msg, params[i]));
            teacherResults.push_back(buf);
        } else {
            teacherResultsW = new TeacherResultsWidget(this, teacherResults);
            connect(teacherResultsW->goBack, &QPushButton::clicked, this, [this] {delete teacherResultsW; teacherResults.clear(); setTeacherWindow(); });
            setCentralWidget(teacherResultsW);
        }
    } else if (cmd == "get student tests") {
          if (StringOperator::cutArg(msg, "status") == "sending") {
           studentPlannedTests.push_back({StringOperator::cutArg(msg, "testname"), StringOperator::cutArg(msg, "testsubject"), StringOperator::cutArg(msg, "testplanneddate")});
       } else {
           delete studentW;
           studentTestsW = new StudentTestsWidget(studentPlannedTests, this);
           connect(studentTestsW->goBack, &QPushButton::clicked, this, [this] {delete studentTestsW; setStudentWindow();});
           connect(studentTestsW->start, &QPushButton::clicked, this, [this] {
               QString testname = studentTestsW->getSelectedTest();
              if (testname == "") {
                  showError("Сначала выберите тест");
              } else {
                  state = COMPLETETEST;
                  slotSendToServer("{cmd='view test tasks';testname='" + testname + "';}");
              }
           });
           setCentralWidget(studentTestsW);
       }
     } else if (cmd == "get student results") {
        if (StringOperator::cutArg(msg, "status") == "sending") {
            studentResults.push_back({StringOperator::cutArg(msg, "testsubject"), StringOperator::cutArg(msg, "testname"), StringOperator::cutArg(msg, "testdate"), StringOperator::cutArg(msg, "resultpercent")});
        } else {
            studentsResultW = new StudentResultsWidget(studentResults);
            connect(studentsResultW, &StudentResultsWidget::finished, this, [this] () { delete studentsResultW; setStudentWindow();});
            setCentralWidget(studentsResultW);
        }
    }
}

void MyClient::setAdminPlusWindow() {
    state = ADMINPLUS;
    adminPlusW = new AdminPlusWidget(this);
    connect(adminPlusW->addGroup, &QPushButton::clicked, this, [this] {
        delete adminPlusW;
        agw = new AddGroupWidget(this);
        state = ADDGROUP;

        connect(agw, &AddGroupWidget::sendGroupClicked, this, [this] {
            QString groupTitle = agw->getGroupTitle();
            if (groupTitle == "") {
                showError("Название группы пустое");
                return;
            }
            QString req = "{cmd='add group';";
            req += "groupname='" + groupTitle+"';}";\
            slotSendToServer(req);
        });

        connect(agw, &AddGroupWidget::goBackClicked, this, [this] {delete agw; setAdminPlusWindow();});
        setCentralWidget(agw);});


    connect(adminPlusW->addToGroup, &QPushButton::clicked, this, [this] {
        delete adminPlusW;
        atgw = new AddToGroupWidget(this);
        connect(atgw->sendToGroup, &QPushButton::clicked, this,
                [this] () {
            QString name = atgw->getName();
            QString surname = atgw->getSurame();
            QString title = atgw->getTitle();
            if (name == "" || surname == "" || title == "") {
                showError("Заполните все поля");
                return;
            } else {
                QString msg = "{cmd='add to group';";
                msg += "studentsname='" + name + "';";
                msg += "studentssurname='" + surname + "';";
                msg+= "groupname='" + title + "';}";
                slotSendToServer(msg);
            }
        });
        connect(atgw->goBack, &QPushButton::clicked, this,
                [this] () {delete atgw; setAdminPlusWindow();});
        setCentralWidget(atgw);});


    connect(adminPlusW->appointGroup, &QPushButton::clicked, this, [this] {
        delete adminPlusW;
        appgw = new AppointGroupWidget(this);
        connect(appgw->goBack, &QPushButton::clicked, this,
                [this] () {delete appgw; setAdminPlusWindow();});

        connect(appgw->sendAppointGroup, &QPushButton::clicked, this,
                [this] () {
            slotSendToServer("{cmd='appoint';"
            "teachername='" + appgw->getName() + "';"
            "teachersurname='" + appgw->getSurname() + "';"
            "groupname='" + appgw->getTitle() + "';}");
        });

        setCentralWidget(appgw);});


    connect(adminPlusW->addUser, &QPushButton::clicked, this, [this] {
        delete adminPlusW;  auw = new AddUserWidget(this);
        connect(auw->goBack, &QPushButton::clicked, this,
                [this] () {delete auw; setAdminPlusWindow();});
        connect(auw->addUser, &QPushButton::clicked, this,
                [this] () {
            slotSendToServer(
            "{cmd='add user';"
            "login='" + auw->getLogin() + "';"
            "pass='" + auw->getPassword() + "';"
            "name='" + auw->getName() + "';"
            "surname='" + auw->getSurname() + "';"
            "role='" + auw->getRole() + "';"
            "}");
        });

        setCentralWidget(auw);});

    connect(adminPlusW->goBack, &QPushButton::clicked, this, [this] {delete adminPlusW; setAuthorizationWindow();});


    setCentralWidget(adminPlusW);
}

void MyClient::setAdminWindow() {
    adminW = new AdminWidget(this);

    connect(adminW->results, &QPushButton::clicked, this,
            [this] () {slotSendToServer("{cmd='view all results';}");
    });
    connect(adminW->groups, &QPushButton::clicked, this,
            [this] () {slotSendToServer("{cmd='view all groups';}");});
    connect(adminW->tests, &QPushButton::clicked, this,
            [this] () {slotSendToServer("{cmd='view all planned tests';}");});

    connect(adminW->goBack, &QPushButton::clicked, this,
            [this] () {delete adminW; setAuthorizationWindow();});

    setCentralWidget(adminW);
}

void MyClient::setTeacherWindow() {
    teacherW = new TeacherWidget(this);
    connect(teacherW->newTaskButton, &QPushButton::clicked, this,
            [this] {
        delete teacherW;
        addTaskW = new AddTaskWidget(this);
        connect(addTaskW->quit, &QPushButton::clicked, this, [this] {delete addTaskW; setTeacherWindow();});
        connect(addTaskW->save, &QPushButton::clicked, this, [this] {
            QString task = addTaskW->getTask();
            QString answer = addTaskW->getAnswer();
            QList <QString> answerOptions = addTaskW->getAnswerOptions();
            QString theme = addTaskW->getTheme();
            QString subject = addTaskW->getSubject();

            if (task == "" || answer == "" || theme == "" || subject == "" || answerOptions.isEmpty()) {
                showError("Заполните все поля");
                return;
            } else {
                QString msg = "{cmd='add task';id='" + QString::number(id) + "';tasktext='" + task + "';";
                msg += "answer='" + answer + "';theme='" + theme + "';subject='" + subject + "';answerOptions='";
                for (auto &i : answerOptions)
                    msg += i + ';';
                msg += "';}";
                slotSendToServer(msg);
            } });
        setCentralWidget(addTaskW);});

    connect(teacherW->newTestButton, &QPushButton::clicked, this,
            [this] {
        delete teacherW;
        addTestW = new AddTestWidget(this);
        connect(addTestW, &AddTestWidget::finished, this, [this] {
            if (addTestW->getState() == RANDOM)
                slotSendToServer("{cmd='add test';amount='" + QString::number(addTestW->getTasksAmount()) + "';"
                                  "testname='" + addTestW->getName() + "';"
                                  "taskauthor='" + (addTestW->getTasksAuthor() == "ME" ? QString::number(id) : "ALL") + "';"
                                  "subject='" + addTestW->getSubject() + "';" + "planneddate='" + DateConverter::DateToStringFormat(addTestW->getDate(), "DD-MM-YYYY") + "';"
                                  "teacherid='" + QString::number(id) + "';"
                                  "theme='" + (addTestW->getTheme() == "" ? "ALL" : addTestW->getTheme()) + "';}");
            else
                slotSendToServer("{cmd='add separated test';"
                                 "testname='" + addTestW->getName() + "';"
                                 "subject='" + addTestW->getSubject() + "';" + "planneddate='" + DateConverter::DateToStringFormat(addTestW->getDate(), "DD-MM-YYYY") + "';"
                                 "teacherid='" + QString::number(id) + "';}");
        });
        connect(addTestW, &AddTestWidget::setUpManual, this, [this] {
           slotSendToServer("{cmd='get all tasks';}"); });
        setCentralWidget(addTestW);
    });

    connect(teacherW->viewGroupsButton, &QPushButton::clicked, this,
            [this] {delete teacherW; slotSendToServer("{cmd='get teacher groups';teacherid='" + QString::number(id) + "';}"); });

    connect(teacherW->viewResultsButton, &QPushButton::clicked, this,
            [this] {delete teacherW; slotSendToServer("{cmd='get teacher results';teacherid='" + QString::number(id) + "';}"); });
    connect(teacherW->appointTest, &QPushButton::clicked, this , [this] {
        state = APPOINTTEST;
        teacherGroups.clear();
        allPlannedTestsList.clear();
        slotSendToServer("{cmd='get teacher groups';teacherid='" + QString::number(id) + "';}"); });

    connect(teacherW->goBack, &QPushButton::clicked, this, [this] {delete teacherW; setAuthorizationWindow();});
    setCentralWidget(teacherW);
}

void MyClient::setStudentWindow() {
    studentW = new StudentWidget(this);
    connect(studentW->goBack, &QPushButton::clicked, this, [this] {delete studentW; setAuthorizationWindow(); });
    connect(studentW->currentTests, &QPushButton::clicked, this, [this] {
        studentPlannedTests.clear();
        slotSendToServer("{cmd='get student tests';studentid='"
                         + QString::number(id) + "';}"); });
    connect(studentW->results, &QPushButton::clicked, this, [this] {
        studentResults.clear();
        slotSendToServer("{cmd='get student results';studentid='"
                         + QString::number(id) + "';}");});

    setCentralWidget(studentW);
}

