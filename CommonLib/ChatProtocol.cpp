#include "ChatProtocol.h"

#include <sstream>

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>

#include <QStringLiteral>


static const QString NameKey = QStringLiteral("name");
static const QString TypeKey = QStringLiteral("type");
static const QString TextKey = QStringLiteral("text");
static const QString RoomKey = QStringLiteral("room");
static const QString RoomsKey = QStringLiteral("rooms");
static const QString MessageKey = QStringLiteral("message");
static const QString FromKey = QStringLiteral("from");
static const QString ContactsKey = QStringLiteral("fromid");
static const QString IdKey = QStringLiteral("id");
static const QString ToIdKey = QStringLiteral("toid");
static const QString FromIdKey = QStringLiteral("fromid");
static const QString StatusKey = QStringLiteral("status");


void CommandBase::requestPri(QJsonObject &obj)
{
    obj[TextKey] = QJsonValue(QStringLiteral("Error type"));
}


void CommandBase::responsePri(QJsonObject &obj)
{
    obj[TextKey] = QJsonValue(err_);
}


void CommandBase::parse(QJsonObject &)
{

}


void CommandBase::set_error(const QString &err)
{
    this->err_ = err;
}


QString CommandBase::request()
{
    QJsonObject ret_obj;
    ret_obj[TypeKey] = QJsonValue(cmd_name());
    requestPri(ret_obj);
    return QJsonValue(ret_obj).toString();
}


QString CommandBase::response()
{
    QJsonObject ret_obj;
    ret_obj[TypeKey] = QJsonValue(cmd_name());
    responsePri(ret_obj);
    return QJsonValue(ret_obj).toString();
}


void CommandLogin::requestPri(QJsonObject &obj)
{
    obj[NameKey] = QJsonValue(fromName_);
}


void CommandLogin::responsePri(QJsonObject &obj)
{
    obj[NameKey] = QJsonValue(fromName_);
    obj[IdKey] = QJsonValue(static_cast<int64_t>(fromId_));
}


void CommandLogin::parse(QJsonObject &obj)
{
    fromName_ = obj[NameKey].toString();
}


void CommandPrivate::requestPri(QJsonObject &obj)
{
    obj[RoomKey] = QJsonValue(room);
    obj[MessageKey] = QJsonValue(message);
    obj[FromKey] = QJsonValue(fromName_);
    obj[FromIdKey] = QJsonValue(static_cast<int64_t>(fromId_));
    QJsonArray ids;
    for(const auto &v: to_vi)
        ids.push_back(QJsonValue(static_cast<int64_t>(v)));
    obj[ToIdKey] = ids;
}


void CommandPrivate::responsePri(QJsonObject &obj)
{
    obj[RoomKey] = QJsonValue(room);
    obj[MessageKey] = QJsonValue(message);
    obj[FromKey] = QJsonValue(fromName_);
    obj[FromIdKey] = QJsonValue(static_cast<int64_t>(fromId_));
}


void CommandPrivate::parse(QJsonObject &obj)
{
    message = obj[MessageKey].toString();
    room = obj[RoomKey].toString();
}


bool CommandPrivate::has_id(int id) const
{
    for(const auto &i: to_vi)
        if(i == id) return true;
    return false;
}


void CommandPrivate::add_id(int id)
{
    if(!has_id(id)) to_vi.push_back(id);
}


void CommandPrivate::clear_id()
{
    to_vi.clear();
}


void CommandLogout::requestPri(QJsonObject &obj)
{
    obj[NameKey] = QJsonValue(fromName_);
}


void CommandLogout::responsePri(QJsonObject &obj)
{
    obj[NameKey] = QJsonValue(fromName_);
}


void CommandLogout::parse(QJsonObject &obj)
{
    fromName_ = obj[NameKey].toString();
}


void CommandMessage::requestPri(QJsonObject &obj)
{
    obj[RoomKey] = QJsonValue(room);
    obj[MessageKey] = QJsonValue(message);
    obj[FromKey] = QJsonValue(fromName_);
    obj[FromIdKey] = QJsonValue(static_cast<int64_t>(fromId_));
}


void CommandMessage::responsePri(QJsonObject &obj)
{
    obj[RoomKey] = QJsonValue(room);
    obj[MessageKey] = QJsonValue(message);
    obj[FromKey] = QJsonValue(fromName_);
    obj[FromIdKey] = QJsonValue(static_cast<int64_t>(fromId_));
}


void CommandMessage::parse(QJsonObject &obj)
{
    message = obj[MessageKey].toString();
    room = obj[RoomKey].toString();
}


void CommandContacts::requestPri(QJsonObject &)
{

}


void CommandContacts::responsePri(QJsonObject &obj)
{
    QJsonArray contacts_arr;
    for(const auto &c: contacts) {
        QJsonArray cont;
        cont.push_back(QJsonValue(static_cast<int64_t>(c.first)));
        cont.push_back(QJsonValue(c.second));
        contacts_arr.push_back(QJsonValue(cont));
    }
    obj[ContactsKey] = QJsonValue(contacts_arr);
}


void CommandContacts::parse(QJsonObject &obj)
{
    if (obj.contains(ContactsKey))
    {
        QJsonArray cont_js = obj[ContactsKey].toArray();
        foreach (auto arr, cont_js) {
            int id = static_cast<int>(arr[0].toInt());
            add_contact(id, arr[1].toString());
        }
    }
}


void CommandContacts::add_contact(int id, const QString &name)
{
    contacts[id] = name;
}


Command Command::error_cmd(int from_id, const QString &error)
{
    Command ret(new CommandError(from_id, QString(), error));
    return ret;
}


bool Command::self_test()
{
    int typeindx = 0;
    Command cmd_from_req, cmd_from_resp;
    int from_id = 10;
    QString from_name(QStringLiteral("Client name"));
    bool ret = false;
    for(auto i = std::begin(CmdNames); i != std::end(CmdNames); ++i) {
        CommandBase::TypeCommand type = static_cast<CommandBase::TypeCommand>(typeindx);
        switch (type) {
        case CommandBase::TypeCommand::ErROR:
            cmd_from_req = Command(CommandError(from_id, from_name, QStringLiteral("Error string")).request());
            break;
        case CommandBase::TypeCommand::LOGIN:
            cmd_from_req = Command(CommandLogin(QStringLiteral("Login string")).request());
            break;
        case CommandBase::TypeCommand::LOGOUT:
            cmd_from_req = Command(CommandLogout(QStringLiteral("Login string")).request());
            break;
        case CommandBase::TypeCommand::MESSAGE:
            cmd_from_req = Command(CommandMessage(QStringLiteral("Room string"), QStringLiteral("Message string")).request());
            break;
        case CommandBase::TypeCommand::PRIVATE_MESSAGE:
            cmd_from_req = Command(CommandPrivate(QStringLiteral("Room string"), QStringLiteral("Message string"), {0,1,2,3}).request());
            break;
        case CommandBase::TypeCommand::CONTACTS:
            cmd_from_req = Command(CommandContacts().request());
            break;
        case CommandBase::TypeCommand::ROOMS:
            cmd_from_req = Command(CommandRooms().request());
            break;
        case CommandBase::TypeCommand::CREATE_ROOM:
            cmd_from_req = Command(CommandCreateRoom(QStringLiteral("Room name string")).request());
            break;
        case CommandBase::TypeCommand::REMOVE_ROOM:
            cmd_from_req = Command(CommandRemoveRoom(297).request());
            break;
        case CommandBase::TypeCommand::JOIN_ROOM:
            cmd_from_req = Command(CommandJoinRoom(297).request());
            break;
        case CommandBase::TypeCommand::LEAVE_ROOM:
            cmd_from_req = Command(CommandLeaveRoom(297).request());
            break;
        case CommandBase::TypeCommand::CLIENT_STATUS:
            cmd_from_req = Command(CommandClientStatus().request());
            break;
        default:
            ++typeindx;
            continue;
        }
        ret = cmd_from_req.valid();
        if(!ret) return false;
        cmd_from_req.get<CommandBase>()->set_from(from_id, from_name);
        ++typeindx;
    }
    return ret;
}


Command::Command(int from_id, const QString &from_name, const QString &json)
    : cmd(nullptr)
{
    try {
        auto doc = QJsonDocument::fromJson(json.toUtf8());

        auto obj = doc.object();
        auto type = obj[TypeKey].toString();

        int typeindx = 0;
        for(auto i = std::begin(CmdNames); i != std::end(CmdNames); ++i) {
            if(*i == type) break;
            ++typeindx;
        }
        CommandBase::TypeCommand cmd_type = static_cast<CommandBase::TypeCommand>(typeindx);
        switch (cmd_type) {
        case CommandBase::TypeCommand::ErROR:
            cmd = std::shared_ptr<CommandBase>(new CommandError(from_id, from_name));
            break;
        case CommandBase::TypeCommand::LOGIN:
            cmd = std::shared_ptr<CommandBase>(new CommandLogin());
            break;
        case CommandBase::TypeCommand::LOGOUT:
            cmd = std::shared_ptr<CommandBase>(new CommandLogout());
            break;
        case CommandBase::TypeCommand::MESSAGE:
            cmd = std::shared_ptr<CommandBase>(new CommandMessage());
            break;
        case CommandBase::TypeCommand::PRIVATE_MESSAGE:
            cmd = std::shared_ptr<CommandBase>(new CommandPrivate());
            break;
        case CommandBase::TypeCommand::CONTACTS:
            cmd = std::shared_ptr<CommandBase>(new CommandContacts());
            break;
        case CommandBase::TypeCommand::ROOMS:
            cmd = std::shared_ptr<CommandBase>(new CommandRooms());
            break;
        case CommandBase::TypeCommand::CREATE_ROOM:
            cmd = std::shared_ptr<CommandBase>(new CommandCreateRoom());
            break;
        case CommandBase::TypeCommand::REMOVE_ROOM:
            cmd = std::shared_ptr<CommandBase>(new CommandRemoveRoom());
            break;
        case CommandBase::TypeCommand::JOIN_ROOM:
            cmd = std::shared_ptr<CommandBase>(new CommandJoinRoom());
            break;
        case CommandBase::TypeCommand::LEAVE_ROOM:
            cmd = std::shared_ptr<CommandBase>(new CommandLeaveRoom());
            break;
        case CommandBase::TypeCommand::CLIENT_STATUS:
            cmd = std::shared_ptr<CommandBase>(new CommandClientStatus());
            break;
        default:
            cmd = std::shared_ptr<CommandBase>(new CommandError(from_id, from_name, QStringLiteral("Type not compatible")));
            break;
        }
        cmd->set_from(from_id, from_name);
        cmd->parse(obj);
    } catch (const std::exception &e) {
        cmd = std::shared_ptr<CommandBase>(new CommandError(from_id, from_name, QString::fromUtf8(e.what())));
    }
}


bool Command::is(CommandBase::TypeCommand type)
{
    if(cmd) return cmd->is(type);
    return false;
}


QString Command::request()
{
    if(cmd) return cmd->request();
    return QStringLiteral("{}");
}


QString Command::response()
{
    if(cmd) return cmd->response();
    return QStringLiteral("{}");
}


bool Command::valid()
{
    if(cmd) return cmd->valid();
    return false;
}


const QString Command::error()
{
    if(cmd) return cmd->error();
    return QStringLiteral("null");
}


const QString Command::type()
{
    if(cmd) return cmd->type();
    return QStringLiteral("null");
}


void CommandRooms::requestPri(QJsonObject &)
{
}


void CommandRooms::responsePri(QJsonObject &obj)
{
    QJsonArray rooms_js;
    for(const auto &room: rooms) {
        QJsonObject rm;
        rm[IdKey] = QJsonValue(static_cast<int64_t>(room.first));
        rm[TypeKey] = QJsonValue(room.second.first);
        rm[NameKey] = QJsonValue(room.second.second);
        rooms_js.push_back(QJsonValue(rm));
    }
    obj[RoomsKey] = rooms_js;
}


void CommandRooms::parse(QJsonObject &obj)
{
    if(obj.contains(RoomsKey)) {
        auto rooms_js = obj[RoomsKey].toArray();
        foreach(auto room_js, rooms_js) {
            QJsonObject room_obj = room_js.toObject();
            int id = static_cast<int>(room_obj[IdKey].toInt());
            add_room(id, room_obj[TypeKey].toString(), room_obj[NameKey].toString());
        }
    }
}


void CommandRooms::add_room(int id, const QString &type, const QString &name)
{
    rooms[id] = {type, name};
}


void CommandCreateRoom::requestPri(QJsonObject &obj)
{
    obj[NameKey] = QJsonValue(room);
}


void CommandCreateRoom::responsePri(QJsonObject &obj)
{
    obj[IdKey] = QJsonValue(static_cast<int64_t>(id_room));
    obj[NameKey] = QJsonValue(room);
}


void CommandCreateRoom::parse(QJsonObject &obj)
{
    room = obj[NameKey].toString();

    if (obj.contains(IdKey))
        id_room = static_cast<int>(obj[IdKey].toInt());
}


void CommandRemoveRoom::requestPri(QJsonObject &obj)
{
    obj[IdKey] = QJsonValue(static_cast<int64_t>(id_room));
}


void CommandRemoveRoom::responsePri(QJsonObject &obj)
{
    requestPri(obj);
}


void CommandRemoveRoom::parse(QJsonObject &obj)
{
    id_room = static_cast<int>(obj[IdKey].toInt());
}


void CommandJoinRoom::requestPri(QJsonObject &obj)
{
    obj[IdKey] = QJsonValue(static_cast<int64_t>(id_room));
}


void CommandJoinRoom::responsePri(QJsonObject &obj)
{
    requestPri(obj);
}


void CommandJoinRoom::parse(QJsonObject &obj)
{
    id_room = static_cast<int>(obj[IdKey].toInt());
}


void CommandLeaveRoom::requestPri(QJsonObject &obj)
{
    obj[IdKey] = QJsonValue(static_cast<int64_t>(id_room));
}


void CommandLeaveRoom::responsePri(QJsonObject &obj)
{
    requestPri(obj);
}


void CommandLeaveRoom::parse(QJsonObject &obj)
{
    id_room = static_cast<int>(obj[IdKey].toInt());
}


void CommandClientStatus::requestPri(QJsonObject &)
{

}


void CommandClientStatus::responsePri(QJsonObject &obj)
{
    obj[FromKey] = QJsonValue(fromName_);
    obj[FromIdKey] = QJsonValue(static_cast<int64_t>(fromId_));
    QJsonArray statuses;
    for(auto &status: statuslist) {
        QJsonArray status_js;
        status_js.push_back(QJsonValue(status.first));
        status_js.push_back(QJsonValue(status.second));
        statuses.push_back(QJsonValue(status_js));
    }
    obj[StatusKey] = QJsonValue(statuses);
}


void CommandClientStatus::parse(QJsonObject &obj)
{
    if(obj.contains(StatusKey)) {
        fromName_ = obj[FromKey].toString();
        fromId_ = static_cast<int>(obj[FromIdKey].toInt());
        QJsonArray stat_arr = obj[StatusKey].toArray();
        foreach(auto stat_it, stat_arr) {
            QJsonArray stat_it_arr = stat_it.toArray();
            add_status(stat_it_arr[0].toString(), stat_it_arr[1].toString());
        }
    }
}


void CommandClientStatus::add_status(const QString &type, const QString &value)
{
    statuslist.emplace_back(type, value);
}
