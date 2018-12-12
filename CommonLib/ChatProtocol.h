#ifndef CHATPROTOCOL_H
#define CHATPROTOCOL_H


#include <memory>
#include <map>
#include <vector>


#include <QJsonObject>
#include <QString>


static const QString CmdNames[] = {
    QStringLiteral("er"), // error
    QStringLiteral("in"), // login
    QStringLiteral("ou"), // logout
    QStringLiteral("bm"), // broadcast message
    QStringLiteral("pm"), // private message
    QStringLiteral("cn"), // get contacts
    QStringLiteral("rm"), // get rooms
    QStringLiteral("cr"), // create room
    QStringLiteral("rr"), // remove room
    QStringLiteral("jr"), // join room
    QStringLiteral("lr"), // leave room
    QStringLiteral("cs")  // client status
};


class CommandBase {

public:

    enum class TypeCommand: int{

        ErROR = 0,
        LOGIN,
        LOGOUT,
        MESSAGE,
        PRIVATE_MESSAGE,
        CONTACTS,
        ROOMS,
        CREATE_ROOM,
        REMOVE_ROOM,
        JOIN_ROOM,
        LEAVE_ROOM,
        CLIENT_STATUS,
        NOT_COMPATIBLE
    };

protected:
    TypeCommand cmdType_;
    QString err_;
    int fromId_;
    QString fromName_;

    virtual void requestPri(QJsonObject &obj);
    virtual void responsePri(QJsonObject &obj);

public:
    virtual void parse(QJsonObject &obj);
    static CommandBase parse(int fromId, const QString &fromName, const QString &command);
    CommandBase(int fromId, const QString &fromName, TypeCommand type = TypeCommand::ErROR)
        : cmdType_(type), err_(), fromId_(fromId), fromName_(fromName) {}
    CommandBase(int fromId, const QString &fromName, const QString &error, TypeCommand type = TypeCommand::ErROR)
        : cmdType_(type), err_(error), fromId_(fromId), fromName_(fromName) {}
    CommandBase(int fromId, TypeCommand type = TypeCommand::ErROR)
        : CommandBase(fromId, QString(), type) {}
    CommandBase(TypeCommand type = TypeCommand::ErROR)
        : CommandBase(-1, QString(), type) {}
    virtual ~CommandBase() {}
    void setFrom(int fromId, const QString &fromName) { this->fromId_ = fromId; this->fromName_ = fromName; }
    void setError(const QString &err_);
    QString request();
    QString response();
    bool valid() { return cmdType_ != TypeCommand::ErROR; }
    bool is(TypeCommand type) { return type == cmdType_; }
    const QString& error() { return err_; }
    const QString type() { return QString::number(static_cast<int>(cmdType_)); }
    QString cmdName() const { return CmdNames[static_cast<int>(cmdType_)]; }
};


class CommandLogin: public CommandBase {
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandLogin(const QString& login = QString())
        : CommandBase(-1, login, TypeCommand::LOGIN) {}
    QString login() const { return fromName_; }
};


class CommandLogout: public CommandBase {
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandLogout(const QString& login = QString())
        : CommandBase(-1, login, TypeCommand::LOGOUT) {}
};


class CommandMessage: public CommandBase {
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    QString room, message;
    CommandMessage(const QString &room= QString(), const QString& message = QString())
        : CommandBase(TypeCommand::MESSAGE), room(room), message(message) {}
};


class CommandPrivate: public CommandBase {
    std::vector<int> toVi_;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    QString room_, message_;
    CommandPrivate(const QString &room = QString(),
                   const QString &message = QString(),
                   const std::vector<int> &toid = std::vector<int>())
        : CommandBase(TypeCommand::PRIVATE_MESSAGE), toVi_(toid), room_(room), message_(message) {}
    bool hasId(int id) const;
    void addId(int id);
    void clearId();
};


class CommandContacts: public CommandBase {
    std::map<int, QString> contacts;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandContacts()
        : CommandBase(TypeCommand::CONTACTS) {}
    void addContact(int id, const QString &name);
};


class CommandRooms: public CommandBase {
    std::map<int, std::pair<QString, QString>> rooms;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandRooms()
        : CommandBase(TypeCommand::ROOMS) {}
    void addRoom(int id, const QString &type, const QString &name);
};


class CommandCreateRoom: public CommandBase {
    QString room_;
    int idRoom_;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandCreateRoom(const QString &room = QString())
        : CommandBase(TypeCommand::CREATE_ROOM), room_(room), idRoom_(-1) {}
};


class CommandRemoveRoom: public CommandBase {
protected:
    int idRoom_;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandRemoveRoom(int idRoom = -1)
        : CommandBase(TypeCommand::REMOVE_ROOM), idRoom_(idRoom) {}
};


class CommandJoinRoom: public CommandBase {
protected:
    int idRoom_;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandJoinRoom(int id_room = -1)
        : CommandBase(TypeCommand::JOIN_ROOM), idRoom_(id_room) {}
};


class CommandLeaveRoom: public CommandBase {
protected:
    int idRoom_;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandLeaveRoom(int idRoom = -1)
        : CommandBase(TypeCommand::LEAVE_ROOM), idRoom_(idRoom) {}
};


class CommandClientStatus: public CommandBase {
protected:
    std::vector<std::pair<QString, QString>> statuslist;
    void requestPri(QJsonObject &obj) override;
    void responsePri(QJsonObject &obj) override;
    void parse(QJsonObject &obj) override;
public:
    CommandClientStatus()
        : CommandBase(TypeCommand::CLIENT_STATUS) {}
    void addStatus(const QString &type, const QString &value);
};


using CommandError = CommandBase;


class Command {

    std::shared_ptr<CommandBase> cmd;

public:

    Command() : cmd(nullptr) {}
    Command(CommandBase *command): cmd(std::shared_ptr<CommandBase>(command)) {}
    static Command errorCmd(int fromId, const QString &error);
    static bool selfTest();
    Command(int fromId, const QString &fromName, const QString &json);
    Command(const QString &json)
        : Command(-1, QString(), json) {}
    bool is(CommandBase::TypeCommand type);
    template <typename T>
    T *get() {
        if(!cmd) return nullptr;
        return dynamic_cast<T*>(cmd.get());
    }
    QString request();
    QString response();
    bool valid();
    const QString error();
    const QString type();
};


#endif // CHATPROTOCOL_H
