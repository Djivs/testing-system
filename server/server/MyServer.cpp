// Copyright 2021 Dmitriy Trifonov
#include <QFile>
#include "MyServer.h"

MyServer::MyServer(int nPort, QObject *parent) : QObject(parent), m_nNextBlockSize(0) {
    //  создать сервер
    m_ptcpServer = new QTcpServer(this);

    //  проверить, работавет ли сервер, если нет, вывести почему
    if (!m_ptcpServer->listen(QHostAddress::Any, nPort)) {
            qDebug() << "Server Error";
            qDebug() << "Unable to start the server:";
            qDebug() << m_ptcpServer->errorString();
            m_ptcpServer->close();
            return;
        }

    //  подсоединить слот для новых соединений
    connect(m_ptcpServer, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    qDebug() << "Server started succesfully :)\n";

    // инициализация переменной для общения с БД и обозначение нужных входных данных
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("127.0.0.1");
    db.setDatabaseName("systemdb");
    db.setUserName("postgres");
    db.setPassword("123");

    //  попробовать соединиться с БД
    if (db.open())
        qDebug() << "Succesfully connected to database! :)";
    else
        qDebug() << "Can't connect to DB :(\n";
}

void MyServer::slotNewConnection() {
    //получить сокет нового соединения
    QTcpSocket* pClientSocket = m_ptcpServer->nextPendingConnection();

    qDebug() << "New client connected!";

    //  подсоединить слоты для подключившегося клиента(на чтение и на удаление при выходе)
    connect(pClientSocket, SIGNAL(disconnected()), pClientSocket, SLOT(deleteLater()));
    connect(pClientSocket, SIGNAL(readyRead()), this, SLOT(slotReadClient()));

    //  отправить клиенту подтверждающий сигнал
    sendToClient(pClientSocket, "{cmd='welcome';}");
}

void MyServer::slotReadClient() {
    // получить сокет отправителя
    QTcpSocket* pClientSocket = static_cast<QTcpSocket*>(sender());
    QDataStream in(pClientSocket);
    in.setVersion(QDataStream::Qt_DefaultCompiledVersion);
    //  цикл используется на случай, если не все данные от клиента придут одновременно
    for (;;) {
        //  получить размер следующего блока со всеми очевидными исключениями
        if (!m_nNextBlockSize) {
            if (pClientSocket->bytesAvailable() < static_cast<qint64>(sizeof(quint64))) {
                    break;
                }
                in >> m_nNextBlockSize;
            }

            if (pClientSocket->bytesAvailable() < m_nNextBlockSize) {
                break;
            }


            //  получение сообщения в виде QString
            QTime   time;
            QString str;
            in >> time >> str;
            QString strMessage = str;

            qDebug() << str;

            //  обработка полученного сообщения
            solveMsg(pClientSocket, strMessage);

            //  обш=нуляем размер следующего блока
            m_nNextBlockSize = 0;
        }
}

void MyServer::sendToClient(QTcpSocket* pSocket, const QString& str) {
    //  преобразование строки в нужный формат для отправки
    QByteArray  arrBlock;
    QDataStream out(&arrBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_DefaultCompiledVersion);
    out << quint16(0) << QTime::currentTime() << str;

    out.device()->seek(0);
    out << quint16(arrBlock.size() - sizeof(quint16));

    //  непосредственно отправка
    pSocket->write(arrBlock);
}

void MyServer::solveMsg(QTcpSocket* pSocket, QString msg) {
    //  если пакет составлен неправильно, игнорируем
    if (!StringOperator::validatePackage(msg)) {
        return;
    }
    //  записываем в cmd команд узапроса пользователя
    QString cmd = StringOperator::cutArg(msg, "cmd");
    if (cmd == "authorize") {
        //  авторизация пользователя в системе


        //  получение необходимых строк из запроса
        QString login = StringOperator::cutArg(msg, "login");
        QString pass = StringOperator::cutArg(msg, "pass");

        //  поиск пользователей с отправленным логином и паролем
        QString sqlRequest = "SELECT role, id FROM users WHERE "
                             "login = '" + login + "' AND "
                             "password = '" + pass + "'";
        QSqlQuery query = QSqlQuery(db);

        if (!query.exec(sqlRequest)) {
            printSQLError(query);
        } else {
            //  если такого пользователя нет, отправляем ошибку
            if (query.size() == 0) {
                sendToClient(pSocket, "{cmd='authorize';status='1';}");
            } else {
                // иначе отправляем клиенту посылку с успешным статусом, ролью пользователя и его id
                while (query.next()) {
                    QString str = "{cmd='authorize';status='0';role='";
                    str += query.record().field(0).value().toString() + "';";
                    str += "id='" + query.record().field(1).value().toString()+"';}";
                    sendToClient(pSocket, str);
                }
            }
        }
    } else if (cmd == "add group") {
        //  добавление новой группы


        //  берём из запроса название новой группы
        QString groupName = StringOperator::cutArg(msg, "groupname");
        QSqlQuery query = QSqlQuery(db);

        //  проверяем, есть ли группа с таким же названием
        if (!query.exec("SELECT id FROM groups WHERE name = '" + groupName + "';")) {
            printSQLError(query);
        } else if (query.size()) {
            //  если такая группа есть, отправляем ошибку
            sendToClient(pSocket, "{cmd='add group';staus='group already exists';}");
            return;
        }

        //  добавление группы
        query.prepare("INSERT INTO groups (name) "
                       "VALUES (:name)");
        query.bindValue(":name", groupName);
        if (query.exec())
            //  в случае успешного добавления отправить посылку со статусом 0 клиенту
            sendToClient(pSocket,  "{cmd='add group';status='0';}");
        else
            printSQLError(query);
    } else if (cmd == "add to group") {
        //  добавить пользователя в группу


        int groupId = 0;
        int userId = 0;
        //получаем из запроса нужные данные
        QString name = StringOperator::cutArg(msg, "studentsname");
        QString surname = StringOperator::cutArg(msg, "studentssurname");
        QString title = StringOperator::cutArg(msg, "groupname");

        //  ищем группы с отправленным именем
        QString sqlRequest = "SELECT id FROM groups WHERE name = '" + title + "';";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(sqlRequest)) {
            printSQLError(query);
        } else {
            if (!query.size()) {
                //  в случае ненахода отправить клиенту ошибку
                sendToClient(pSocket, "{cmd='add to group';status='1';}");
                return;
            } else {
                //  иначе сохранить id группы
                while (query.next())
                    groupId = query.record().field(0).value().toInt();
            }
        }

        //  ищем пользователей с введёнными именем и фамилией
        sqlRequest = "SELECT id FROM users WHERE name = '" + name;
        sqlRequest += "' AND surname = '" + surname + "';";
        if (!query.exec(sqlRequest)) {
            printSQLError(query);
        } else {
            if (!query.size()) {
                //  если таких польщователей нет, отправить ошибку
                sendToClient(pSocket, "{cmd='add to group';status='2';}");
                return;
            } else {
                //  если пользователь найден, записать его id
                while (query.next())
                    userId = query.record().field(0).value().toInt();
            }
        }

        //  проверяем, есть ли уже этот пользователь в группе с названием из запроса
        //  если есть, отправить ошибку
        sqlRequest = "SELECT group_by_user.userId FROM group_by_user"
                     " INNER JOIN users ON users.id = group_by_user.userId"
                     " INNER JOIN groups ON groups.id = group_by_user.groupId"
                     " WHERE users.id = " + QString::number(userId) + " AND"
                     " groups.id = " + QString::number(groupId) + ';';
        if (!query.exec(sqlRequest)) {
            printSQLError(query);
        } else if (query.size()) {
            sendToClient(pSocket, "{cmd='add to group';status='3';}");
            return;
        }

        //  добавить пользователя в группу
        //  в случае удачи отправить пользователю статус 0
        query.prepare("INSERT INTO group_by_user (userId, groupId) "
                      "VALUES (:userId, :groupId)");
        query.bindValue(":userId", userId);
        query.bindValue(":groupId", groupId);
        if (query.exec())
            sendToClient(pSocket,  "{cmd='add to group';status='0';}");
        else
            printSQLError(query);
    } else if (cmd == "appoint") {
        //  назначить группу преподавателю


        //  получаем необходимые данные из запроса
        QString teacherName = StringOperator::cutArg(msg, "teachername");
        QString teacherSurname = StringOperator::cutArg(msg, "teachersurname");
        QString groupName = StringOperator::cutArg(msg, "groupname");
        int teacherId = 0;

        //  ищем учителя с данными из запроса
        //  если таких учителейц нет, выдаём ошибку
        //  иначе записываем id учителя
        QString sqlRequest = "SELECT id FROM users WHERE "
                             "role = 'teacher' AND name = '" + teacherName + "'"
                             " AND surname = '" + teacherSurname + "';";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(sqlRequest)) {
            printSQLError(query);
        } else if (!query.size()) {
            sendToClient(pSocket, "{cmd='appoint';status='1';}");
            return;
        } else {
            while (query.next())
                teacherId = query.record().field(0).value().toInt();
        }

        //  проверяем, есть ли у группы с именем groupName  назначенный учитель
        //  если есть, отправляем ошибку
        sqlRequest = "SELECT id FROM groups WHERE name = '" + groupName + "' AND teacherid IS NULL;";
        if (!query.exec(sqlRequest)) {
            printSQLError(query);
        } else if (!query.size()) {
            sendToClient(pSocket, "{cmd='appoint';status='2';}");
            return;
        }

        //  обновляем таблицу с группами и ставим id учителя
        //  если всё прошло успешно отправляем клиенту код 0
        query.prepare("UPDATE groups SET teacherid = (:teacherid) "
                        "WHERE name = '" + groupName + "';");
        query.bindValue(":teacherid", teacherId);
        if (query.exec())
            sendToClient(pSocket, "{cmd='appoint';status='0';}");
        else
            printSQLError(query);
    } else if (cmd == "add user") {
        //  добавить нового пользователя в систему


        //  получаем нужные данные из запроса
        QString name = StringOperator::cutArg(msg, "name");
        QString surname = StringOperator::cutArg(msg, "surname");
        QString login = StringOperator::cutArg(msg, "login");
        QString pass = StringOperator::cutArg(msg, "pass");
        QString role = StringOperator::cutArg(msg, "role");

        //  проверяем, есть ли пользователь с такими же данными
        //  если есть, отправляем ошибку
        QString sqlRequest = "SELECT id FROM users WHERE "
                             "(name = '" + name + "' AND "
                             "surname = '" + surname + "' AND "
                             "login = '" + login + "' AND "
                             "password = '" + pass + "' AND "
                             "role = '" + role + "') OR (login = '" + login + "' AND password = '" + pass + "');";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(sqlRequest)) {
            printSQLError(query);
        } else if (query.size()) {
            sendToClient(pSocket, "{cmd='add user', status='1'");
            return;
        }

        //  добавляем пользователя
        //  если все прошло успешно отправляем код 0
        query.prepare("INSERT INTO users (login, password, name, surname, role)"
                        "VALUES (:login, :password, :name, :surname, :role);");
        query.bindValue(":login", login);
        query.bindValue(":password", pass);
        query.bindValue(":name", name);
        query.bindValue(":surname", surname);
        query.bindValue(":role", role);

        if (!query.exec())
            printSQLError(query);
        else
            sendToClient(pSocket, "{cmd='add user', status='0'");
    } else if (cmd == "view all results") {
        //  просмотреть ВСЕ результаты



        //  оповещаем клиента о начале отправки данных
        sendToClient(pSocket, "{cmd='view all results';status='started';}");

        //  выбираем все данные о результатах и по одному query.record() отсылаем клиенту

        QString req = "SELECT tests.name, tests.subject, users.name, users.surname, results.percent "
                      "FROM results INNER JOIN users ON results.studentid = users.id "
                      "INNER JOIN tests ON results.testid = tests.id;";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req)) {
            printSQLError(query);
        } else {
            QList <QString> params = {"testname", "subject", "studentsname", "studentssurname", "percent"};
            while (query.next()) {
                QString processMsg = "{cmd='view all results';";
                for (int i = 0; i < 5; ++i)
                    processMsg += params[i] + "='" + query.record().field(i).value().toString() + "';";
                processMsg += '}';
                sendToClient(pSocket, processMsg);
            }
        }

        //  оповещаем клиента о том, что все данные высланы
        sendToClient(pSocket, "{cmd='view all results';status='sended';}");
    } else if (cmd == "view all groups") {
        //  просмотр всех групп


        //  делавем запрос по данным всех групп и отсылаем по одному query.record() пользователюЫ
        QString req = "SELECT groups.name, users.name, users.surname FROM groups INNER JOIN users ON users.id = groups.teacherid";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req)) {
            printSQLError(query);
        } else {
            while (query.next()) {
                QString teacherName = query.record().field(1).value().toString() + " " + query.record().field(2).value().toString();
                QString groupName = query.record().field(0).value().toString();
                QString processMsg = "{cmd='view all groups';name='" + groupName + "';teachername='" + teacherName + "';}";
                sendToClient(pSocket, processMsg);
            }
            sendToClient(pSocket, "{cmd='view all groups';status='sended';}");
        }
    } else if (cmd == "view group students") {
        QString req = "SELECT users.name, users.surname FROM group_by_user "
                      "INNER JOIN users ON group_by_user.userid = users.id "
                      "INNER JOIN groups ON group_by_user.groupid = groups.id "
                      "WHERE groups.name = '" + StringOperator::cutArg(msg, "groupname") + "';";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req)) {
            printSQLError(query);
        } else {
            while (query.next()) {
                QString procMsg = "{cmd='view group students';name='" + query.record().field(0).value().toString();
                procMsg += "';surname='" + query.record().field(1).value().toString() + "';}";
                sendToClient(pSocket, procMsg);
            }
            sendToClient(pSocket, "{cmd='view group students';status='sended';}");
        }
    } else if (cmd == "view all planned tests") {
        QString req = "SELECT users.name, users.surname, tests.name, tests.subject, tests.planneddate "
                      "FROM tests INNER JOIN users ON tests.teacherid = users.id;";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req)) {
            printSQLError(query);
        } else {
            while (query.next()) {
                QString procMsg = "{cmd='view all planned tests';";
                QList <QString> params = {"teachername", "teachersurname", "testname", "subject", "date"};
                for (int i = 0; i < query.record().count(); ++i)
                    procMsg += params[i] + "='" + query.record().field(i).value().toString() + "';";
                procMsg += '}';
                sendToClient(pSocket, procMsg);
            }
            sendToClient(pSocket, "{cmd='view all planned tests';status='sended';}");
        }
    } else if (cmd == "view test tasks") {
        sendToClient(pSocket, "{cmd='view test tasks';status='started';}");
        QString req = "SELECT tests.name, tasks.task, tasks.answeroptions, tasks.answer, tasks.theme, images.id "
                      "FROM test_by_task INNER JOIN tests ON tests.id = test_by_task.testid "
                      "INNER JOIN tasks ON tasks.id = test_by_task.taskid INNER JOIN images on tasks.imageid = images.id "
                      "WHERE tests.name = '" + StringOperator::cutArg(msg, "testname") + "';";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req)) {
            printSQLError(query);
        } else {
            while (query.next()) {
                QString str = "{cmd='view test tasks';";
                QList <QString> params = {"testname", "taskname", "answeroptions", "answertext", "theme", "imageid"};
                for (int i = 0; i < query.record().count(); ++i)
                    str += params[i] + "='" + query.record().field(i).value().toString() + "';";
                str += "}";
                sendToClient(pSocket, str);
            }
            sendToClient(pSocket, "{cmd='view test tasks';status='sended';}");
        }
    } else if (cmd == "add task") {
        qDebug() << 1;
        quint16 id = StringOperator::cutArg(msg, "id").toInt();
        QString tasktext = StringOperator::cutArg(msg, "tasktext");
        QString answer = StringOperator::cutArg(msg, "answer");
        QString theme = StringOperator::cutArg(msg, "theme");
        QString subject = StringOperator::cutArg(msg, "subject");
        QString answerOptions = StringOperator::cutArg(msg, "answerOptions");

        QSqlQuery query = QSqlQuery(db);

        int taskId = 1;
        if (!query.exec("SELECT COUNT(*) FROM tasks"))
            printSQLError(query);
        else if (query.next())
            taskId += query.record().field(0).value().toInt();

        query.prepare("INSERT INTO tasks (task, answeroptions, answer, theme, subject, teacherid) "\
                        "VALUES (:task, :answeroptions, :answer, :theme, :subject, :teacherid);");
        query.bindValue(":task", tasktext);
        query.bindValue(":answeroptions", answerOptions);
        query.bindValue(":answer", answer);
        query.bindValue(":theme", theme);
        query.bindValue(":subject", subject);
        query.bindValue(":teacherid", id);
        if (!query.exec())
            printSQLError(query);
        else
            sendToClient(pSocket, "{cmd='add task';status='sended';taskid='" + QString::number(taskId) + "';}");
    } else if (cmd == "add task image") {
        QSqlQuery query = QSqlQuery(db);
        int imageId = 1;
        if (!query.exec("SELECT COUNT(*) FROM images"))
            printSQLError(query);
        else if (query.next())
            imageId += query.record().field(0).value().toInt();

        QString sender = StringOperator::cutArg(msg, "sender");
        QString taskid = StringOperator::cutArg(msg, "taskid");
        query.prepare("INSERT INTO images (userid) VALUES (:userid)");
        query.bindValue(":userid", sender);
        if (!query.exec())
            printSQLError(query);
        qDebug() << 6;
        query.prepare("UPDATE tasks SET imageid = :imageid WHERE id = " + taskid);
        query.bindValue(":imageid", imageId);
        if (!query.exec())
            printSQLError(query);
        sendToClient(pSocket, "{cmd='add task image';imageid='" + QString::number(imageId) + "';}");
    } else if (cmd == "get all tasks") {
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec("SELECT * FROM tasks")) {
            printSQLError(query);
        } else {
            while (query.next()) {
            QString msg = "{cmd='get all tasks';status='sending'";
            QList <QString> params = {"taskid", "subject", "tasktext", "answeroptions", "answertext", "theme", "teacherid"};
            for (int i = 0; i < 7; ++i)
                msg += params[i] + "='" + query.record().field(i).value().toString() + "';";
            msg += "}";
            sendToClient(pSocket, msg); }
        }
        sendToClient(pSocket, "{cmd='get all tasks';status='sended';}");

    } else if (cmd == "add test") {
        QString testName = StringOperator::cutArg(msg, "testname");
        QString subject = StringOperator::cutArg(msg, "subject");
        QString plannedDate = StringOperator::cutArg(msg, "planneddate");
        QString teacherid = StringOperator::cutArg(msg, "teacherid");

        int testId = 1;
        QSqlQuery query = QSqlQuery(db);
        query.prepare("INSERT INTO tests (name, subject, planneddate, teacherid) "
                                        "VALUES (:name, :subject, :planneddate, :teacherid);");
        query.bindValue(":name", testName);
        query.bindValue(":subject", subject);
        query.bindValue(":planneddate", plannedDate);
        query.bindValue(":teacherid", teacherid);

        if (!query.exec())
            printSQLError(query);
        if (!query.exec("SELECT COUNT(*) FROM tests"))
            printSQLError(query);
        else if (query.next())
            testId = query.record().field(0).value().toInt();
        QString amount = StringOperator::cutArg(msg, "amount");
        QString theme = StringOperator::cutArg(msg, "theme");
        QString taskAuthor = StringOperator::cutArg(msg, "taskauthor");

        QList <int> taskId;

        QString req = "SELECT id FROM tasks WHERE subject = '" + subject + "'";
        if (theme != "ALL")
            req += " AND theme = '" + theme + "'";
        if (taskAuthor != "ALL")
            req += " AND teacherid=" + taskAuthor;
        req += ';';
        if (!query.exec(req))
            printSQLError(query);
        else
            while (query.next())
            taskId.push_back(query.record().field(0).value().toInt());
        for (int i = 0; i < taskId.size(); ++i)
            std::swap(taskId[i], taskId[QRandomGenerator::global()->generate() % taskId.size()]);
        if (taskId.size() <= amount.toInt()) {
            for (auto &i : taskId) {
                query.prepare("INSERT INTO test_by_task (testid, taskid) "
                              "VALUES (:testid, :taskid)");
                query.bindValue(":testid", testId);
                query.bindValue(":taskid", i);
                if (!query.exec())
                    printSQLError(query);
            }
        } else {
            for (int i = 0; i < amount.toInt(); ++i) {
                query.prepare("INSERT INTO test_by_task (testid, taskid) "
                              "VALUES (:testid, :taskid)");
                query.bindValue(":testid", testId);
                query.bindValue(":taskid", taskId[i]);
                if (!query.exec())
                    printSQLError(query);
            }
        }
        sendToClient(pSocket, "{cmd='add test';status='sended';}");
    } else if (cmd == "add separated test") {
        QString testName = StringOperator::cutArg(msg, "testname");
        QString subject = StringOperator::cutArg(msg, "subject");
        QString plannedDate = StringOperator::cutArg(msg, "planneddate");
        QString teacherid = StringOperator::cutArg(msg, "teacherid");
        QSqlQuery query = QSqlQuery(db);
        query.prepare("INSERT INTO tests (name, subject, planneddate, teacherid) "
                                        "VALUES (:name, :subject, :planneddate, :teacherid);");
        query.bindValue(":name", testName);
        query.bindValue(":subject", subject);
        query.bindValue(":planneddate", plannedDate);
        query.bindValue(":teacherid", teacherid);

        if (!query.exec())
            printSQLError(query);

        if (!query.exec("SELECT COUNT(*) FROM tests"))
            printSQLError(query);
        else if (query.next())
            sendToClient(pSocket, "{cmd='add separated test';testid='" + query.record().field(0).value().toString() + "';}");

    } else if (cmd == "appoint task to test") {
        QString testId = StringOperator::cutArg(msg, "testid");
        QString taskId = StringOperator::cutArg(msg, "taskid");
        QSqlQuery query = QSqlQuery(db);
        query.prepare("INSERT INTO test_by_task (testid, taskid) VALUES (:testid, :taskid);");
        query.bindValue(":testid", testId);
        query.bindValue(":taskid", taskId);
        if (!query.exec())
            printSQLError(query);
    } else if (cmd == "get teacher groups") {
        QString teacherid = StringOperator::cutArg(msg, "teacherid");
        QString req = "SELECT name FROM groups WHERE teacherid='" + teacherid + "';";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req))
            printSQLError(query);
        else
            while (query.next())
            sendToClient(pSocket, "{cmd='get teacher groups';status='sending';groupname='" + query.record().field(0).value().toString() + "';}");
        sendToClient(pSocket, "{cmd='get teacher groups';status='sended';}");
    } else if (cmd == "get teacher results") {
        QString id = StringOperator::cutArg(msg, "teacherid");
        QString req = "SELECT users.name, users.surname, groups.name,  tests.name, results.percent, tests.subject, tests.planneddate "
                "FROM users, results, group_by_user, groups, tests "
                "WHERE group_by_user.groupid = groups.id AND groups.teacherid = " + id +  " AND group_by_user.userid = users.id "
                "AND tests.id = results.testid "
                "AND results.studentid = users.id; ";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req)) {
            printSQLError(query);
        } else {
            while (query.next()) {
            QString msg = "{cmd='" + cmd + "';status='sending';";
            QList <QString> params = {"studentsname", "studentssurname", "studentsgroup", "testname", "percent", "subject", "date"};
            for (int i = 0; i < params.size(); ++i)
                msg += params[i] + "='" + query.record().field(i).value().toString() + "';";
            msg += "}";
            sendToClient(pSocket, msg); }
        }
        sendToClient(pSocket, "{cmd='" + cmd + "';status='sended';}");
    } else if (cmd == "appoint test") {
        QString group = StringOperator::cutArg(msg, "groupname");
        QString test = StringOperator::cutArg(msg, "testname");

        int testId = 0;
        int groupId = 0;
        QSqlQuery query = QSqlQuery(db);

        QString req = "SELECT id FROM tests WHERE name='" + test + "';";
        if (!query.exec(req))
            printSQLError(query);
        else if (query.next())
            testId = query.record().field(0).value().toInt();
        req = "SELECT id FROM groups WHERE name='" + group + "';";
        if (!query.exec(req))
            printSQLError(query);
        else if (query.next())
            groupId = query.record().field(0).value().toInt();
        query.prepare("INSERT INTO test_by_group (testid, groupid) VALUES (:testid, :groupid);");
        query.bindValue(":testid", testId);
        query.bindValue(":groupid", groupId);

        if (!query.exec())
            printSQLError(query);
    } else if (cmd == "get student tests") {
        QString id = StringOperator::cutArg(msg, "studentid");
        QString req = "SELECT groupid FROM group_by_user WHERE userid = " + id + ";";
        QStringList groupIds;
        QSet <QString> testIds;
        QList <QList <QString>> result;
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req))
            printSQLError(query);
        else
            while (query.next())
            groupIds.push_back(query.record().field(0).value().toString());
        for (auto &i : groupIds) {
            QString req = "SELECT testid FROM test_by_group WHERE groupid = " + i;
            if (!query.exec(req))
                printSQLError(query);
            else
                while (query.next())
                testIds.insert(query.record().field(0).value().toString());
        }
        for (auto &i : testIds) {
            QString req = "SELECT name, subject, planneddate FROM tests WHERE id = " + i + " AND planneddate >= '" +
                    DateConverter::DateToStringFormat(QDate::currentDate(), "DD-MM-YYYY") + "';";
            if (!query.exec(req)) {
                printSQLError(query);
            } else {
                while (query.next())
                    result.push_back({query.record().field(0).value().toString(),
                    query.record().field(1).value().toString(),
                    query.record().field(2).value().toString()});
            }
        }

        for (auto &i : result) {
            QString msg = "{cmd='get student tests';status='sending';testname='" + i[0] + "';testsubject='" + i[1] + "';testplanneddate='" + i[2] + "';}";
            sendToClient(pSocket, msg);
        }
        sendToClient(pSocket, "{cmd='get student tests';status='sended';}");
    } else if (cmd == "add result") {
        QString testname = StringOperator::cutArg(msg, "testname");
        QString studentId = StringOperator::cutArg(msg, "studentid");
        QString percent = StringOperator::cutArg(msg, "completionpercent");
        QSqlQuery query = QSqlQuery(db);
        QString testid;
        if (!query.exec("SELECT id FROM tests WHERE name = '" + testname + "';"))
            printSQLError(query);
        else if (query.next())
            testid = query.record().field(0).value().toString();

        query.prepare("INSERT INTO results (percent, studentid, testid) VALUES (:percent, :studentid, :testid);");
        query.bindValue(":percent", percent.toInt());
        query.bindValue(":studentid", studentId.toInt());
        query.bindValue(":testid", testid);

        if (!query.exec())
            printSQLError(query);
    } else if (cmd == "get student results") {
        QString studentid = StringOperator::cutArg(msg, "studentid");
        QString req = "SELECT tests.subject, tests.name, tests.planneddate, results.percent "
                       "FROM results "
                       "INNER JOIN tests ON results.testid = tests.id "
                       "WHERE studentid =" + studentid + "3 ORDER BY tests.planneddate DESC;";
        QSqlQuery query = QSqlQuery(db);
        if (!query.exec(req)) {
            printSQLError(query);
        } else {
            while (query.next()) {
            const QStringList params = {"testsubject", "testname", "testdate", "resultpercent"};
            QString msg = "{cmd='get student results';status='sending';";
            for (int i = 0; i < params.size(); ++i) {
                msg += params[i] + "='" + query.record().field(i).value().toString() + "';";
            }
            msg += "}";
            sendToClient(pSocket, msg); }
        }
        sendToClient(pSocket, "{cmd='get student results';status='sended';}");
    }
}
