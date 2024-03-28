#include "TGBotWithDB.h"

const std::string ADMIN_STATUS = "administrator";
const std::string CREATOR_STATUS = "creator";
const std::string ERROR_MESSAGE = u8"Не вмію я так";

void extractUserInfo(TgBot::Message::Ptr message, int64_t& chatId, int64_t& userId, std::string& userName) {
    chatId = message->chat->id;
    userId = message->from->id;
    userName = message->from->username;

    if (message->replyToMessage) {
        userId = message->replyToMessage->from->id;
        userName = message->replyToMessage->from->username;
    }
}

bool isAdminOrCreator(TgBot::Bot& bot, int64_t chatId, int64_t userId) {
    TgBot::ChatMember::Ptr chatMember = bot.getApi().getChatMember(chatId, userId);
    return (chatMember->status == ADMIN_STATUS || chatMember->status == CREATOR_STATUS);
}

bool isMessageTooOld(TgBot::Message::Ptr message, int thresholdInSeconds) {
    time_t now = std::time(0);
    return (now - message->date > thresholdInSeconds);
}

std::string boardToString(const std::vector<std::vector<std::string>>& board) {
    std::string boardStr;
    for (const auto& row : board) {
        for (const auto& cell : row) {
            boardStr += cell + " ";
        }
        boardStr += "\n";
    }
    return boardStr;
}

bool checkWin(const std::vector<std::vector<std::string>>& board) {
    for (size_t i = 0; i < 3; i++) {
        if (board[i][0] != " " && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
            return true;
        }
        if (board[0][i] != " " && board[0][i] == board[1][i] && board[1][i] == board[2][i]) {
            return true;
        }
    }
    if (board[0][0] != " " && board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
        return true;
    }
    if (board[0][2] != " " && board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
        return true;
    }
    return false;
}

struct GameSession {
    int gameId = 0;
    int64_t chatId = 0;
    int64_t player1Id = 0;
    int64_t player2Id = 0;
    std::string player1Name;
    std::string player2Name;
    std::string player1Symbol;
    std::string player2Symbol;
    std::vector<std::vector<std::string>> board;
    int64_t currentPlayerId = 0;
    std::string winnerName;
    std::int32_t messageId = 0;
};

std::map<int, GameSession> gameSessions;

void handleGameStart(TgBot::Bot& bot, TgBot::Message::Ptr message) {
    if (isMessageTooOld(message, 60)) return;

    int64_t chatId = message->chat->id;
    int64_t userId = message->from->id;
    std::string userName = message->from->username;

    int gameId = std::rand() % 2147483647 - 1073741823;

    GameSession& session = gameSessions[gameId];
    session.gameId = gameId;
    session.chatId = chatId;
    session.player1Id = userId;
    session.player1Name = userName;
    session.board = std::vector<std::vector<std::string>>(3, std::vector<std::string>(3, " "));

    TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
    std::vector<TgBot::InlineKeyboardButton::Ptr> row;
    TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
    button->text = u8"Прийняти виклик";
    button->callbackData = "gameId:" + std::to_string(gameId);
    row.push_back(button);
    keyboard->inlineKeyboard.push_back(row);

    TgBot::Message::Ptr sentMessage = bot.getApi().sendMessage(chatId, u8"Гру розпочато! Хто прийме виклик?", false, 0, keyboard);
    session.messageId = sentMessage->messageId;
}

void handleCallbackQuery(TgBot::Bot& bot, TgBot::CallbackQuery::Ptr query, SQLite::Database& db) {
    int64_t chatId = query->message->chat->id;
    int64_t userId = query->from->id;
    std::string userName = query->from->username;
    int64_t messageId = query->message->messageId;

    std::string prefix = "gameId:";
    if (query->data.rfind(prefix, 0) == 0) {
        std::string gameIdStr = query->data.substr(prefix.size());
        try {
            int gameId = std::stoi(gameIdStr);

            if (gameSessions.find(gameId) != gameSessions.end()) {
                GameSession& session = gameSessions[gameId];

                session.player2Id = userId;
                session.player2Name = userName;

                if (std::rand() % 2 == 0) {
                    session.player1Symbol = "X";
                    session.player2Symbol = "O";
                }
                else {
                    session.player1Symbol = "O";
                    session.player2Symbol = "X";
                }

                if (std::rand() % 2 == 0) {
                    session.currentPlayerId = session.player1Id;
                }
                else {
                    session.currentPlayerId = session.player2Id;
                }

                TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
                for (int i = 0; i < 3; i++) {
                    std::vector<TgBot::InlineKeyboardButton::Ptr> row;
                    for (int j = 0; j < 3; j++) {
                        TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
                        button->text = session.board[i][j];
                        button->callbackData = "gameId:" + std::to_string(gameId) + ":cell:" + std::to_string(i) + std::to_string(j);                        row.push_back(button);
                    }
                    keyboard->inlineKeyboard.push_back(row);
                }

                std::string boardStr = boardToString(session.board);
                bot.getApi().editMessageText(u8"Гра розпочалася! " + std::string(session.currentPlayerId == session.player1Id ? session.player1Name : session.player2Name) + u8" ходить.\n" + boardStr, session.chatId, session.messageId, "", "Markdown", false, keyboard);
            }
        }
        catch (std::invalid_argument& e) {
            bot.getApi().sendMessage(chatId, u8"Неправильний формат gameId");
        }
        catch (std::out_of_range& e) {
            bot.getApi().sendMessage(chatId, u8"gameId виходить за допустимі межі");
        }
    }

    std::string cellPrefix = ":cell:";
    if (query->data.rfind(prefix, 0) == 0) {
        std::string gameIdStr = query->data.substr(prefix.size());
        try {
            int gameId = std::stoi(gameIdStr);

            if (gameSessions.find(gameId) != gameSessions.end()) {
                GameSession& session = gameSessions[gameId];

                session.player2Id = userId;
                session.player2Name = userName;

                if (std::rand() % 2 == 0) {
                    session.player1Symbol = "X";
                    session.player2Symbol = "O";
                }
                else {
                    session.player1Symbol = "O";
                    session.player2Symbol = "X";
                }

                if (std::rand() % 2 == 0) {
                    session.currentPlayerId = session.player1Id;
                }
                else {
                    session.currentPlayerId = session.player2Id;
                }

                TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
                for (int i = 0; i < 3; i++) {
                    std::vector<TgBot::InlineKeyboardButton::Ptr> row;
                    for (int j = 0; j < 3; j++) {
                        TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
                        button->text = session.board[i][j];
                        button->callbackData = "gameId:" + std::to_string(gameId) + ":cell:" + std::to_string(i) + std::to_string(j);
                        row.push_back(button);
                    }
                    keyboard->inlineKeyboard.push_back(row);
                }

                std::string boardStr = boardToString(session.board);
                bot.getApi().editMessageText(u8"Гра розпочалася! " + std::string(session.currentPlayerId == session.player1Id ? session.player1Name : session.player2Name) + u8" ходить.\n" + boardStr, session.chatId, session.messageId, "", "Markdown", false, keyboard);
            }
        }
        catch (std::invalid_argument& e) {
            bot.getApi().sendMessage(chatId, u8"Неправильний формат gameId");
        }
        catch (std::out_of_range& e) {
            bot.getApi().sendMessage(chatId, u8"gameId виходить за допустимі межі");
        }
    }

    if (query->data.rfind(prefix, 0) == 0) {
        size_t cellPos = query->data.find(cellPrefix);
        if (cellPos != std::string::npos) {
            std::string gameIdStr = query->data.substr(prefix.size(), cellPos - prefix.size());
            try {
                int gameId = std::stoi(gameIdStr);

                if (gameSessions.find(gameId) != gameSessions.end()) {
                    GameSession& session = gameSessions[gameId];

                    std::string cellData = query->data.substr(cellPos + cellPrefix.size());
                    if (cellData.size() == 2 && cellData[0] >= '0' && cellData[0] <= '2' && cellData[1] >= '0' && cellData[1] <= '2') {
                        int row = cellData[0] - '0';
                        int col = cellData[1] - '0';

                        if (session.currentPlayerId == userId && session.board[row][col] == " ") {

                            if (session.board[row][col] == " ") {
                                session.board[row][col] = session.currentPlayerId == session.player1Id ? session.player1Symbol : session.player2Symbol;

                                std::string boardStr = boardToString(session.board);
                                bot.getApi().editMessageText(u8"Гра розпочалася! " + std::string(session.currentPlayerId == session.player1Id ? session.player1Name : session.player2Name) + u8" ходить.\n" + boardStr, session.chatId, session.messageId);

                                if (checkWin(session.board)) {
                                    SQLite::Statement query(db, "UPDATE player_ids SET wins = wins + 1 WHERE id = ?");
                                    query.bind(1, session.currentPlayerId);
                                    query.exec();
                                    session.winnerName = session.currentPlayerId == session.player1Id ? "Player 1" : "Player 2";

                                    gameSessions.erase(gameId);
                                    return;
                                }

                                session.currentPlayerId = session.currentPlayerId == session.player1Id ? session.player2Id : session.player1Id;
                            }
                        }

                        if (checkWin(session.board)) {
                            SQLite::Statement dbquery(db, "UPDATE player_ids SET wins = wins + 1 WHERE id = ?");
                            dbquery.bind(1, session.currentPlayerId);
                            dbquery.exec();

                            session.winnerName = session.currentPlayerId == session.player1Id ? "Player 1" : "Player 2";

                            std::string boardStr = boardToString(session.board);
                            bot.getApi().sendMessage(chatId, u8"Переможець: " + session.winnerName + u8"\nФінальна дошка:\n" + boardStr);

                            gameSessions.erase(gameId);
                        }
                    }
                }
            }
            catch (std::invalid_argument& e) {
                bot.getApi().sendMessage(chatId, u8"Неправильний формат gameId");
            }
            catch (std::out_of_range& e) {
                bot.getApi().sendMessage(chatId, u8"gameId виходить за допустимі межі");
            }
        }
    }
}

int main() {
    SQLite::Database db("mydb.sqlite", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    db.exec("CREATE TABLE IF NOT EXISTS chat_ids (id INTEGER PRIMARY KEY, settings TEXT)");

    db.exec("CREATE TABLE IF NOT EXISTS player_ids (id INTEGER PRIMARY KEY, wins INTEGER, losses INTEGER, settings TEXT)");

    std::vector<std::string> bot_commands = { "start", "test", "kick", "ban", "mute", "mutemedia", "unmute", "kick", "help" }; // список команд бота

    TgBot::Bot bot("");

    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        bot.getApi().sendMessage(message->chat->id, u8"Привіт!");
        });

    bot.getEvents().onCommand("help", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        bot.getApi().sendMessage(message->chat->id, u8"Список команд бота: \n\n/ban - блокує користувача \n/mute - забороняє писати будь-які повідомлення\n/unmute - знімає всі обмеження з користувача\n/kick - вигоняє людину з чату\n/mutemedia - забороняє надсилати медіа.");
        });

    bot.getEvents().onCommand("ban", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        int64_t chatId = message->chat->id;
        int64_t userId = message->from->id;
        std::string userName = message->from->username;

        std::istringstream iss(message->text);
        std::string command, userTag, daysStr, farewellMessage;
        iss >> command >> userTag >> daysStr;
        std::getline(iss, farewellMessage);

        if (message->replyToMessage) {
            userId = message->replyToMessage->from->id;
            userName = message->replyToMessage->from->username;
        }

        try {
            if (isAdminOrCreator(bot, chatId, userId)) {
                bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
                return;
            }

            if (daysStr.empty()) {
                bot.getApi().banChatMember(chatId, userId);
                farewellMessage = userName + u8" заблоковано назавжди";
            }
            else {
                int days = std::stoi(daysStr);
                time_t currentTime = std::time(0);
                int untilDate = static_cast<int>(currentTime) + days * 24 * 60 * 60;
                bot.getApi().banChatMember(chatId, userId, untilDate);
                farewellMessage = userName + u8" заблоковано на " + daysStr;
                if (daysStr == "1") {
                    farewellMessage += u8" день";
                }
                else if (daysStr == "2" || daysStr == "3" || daysStr == "4") {
                    farewellMessage += u8" дні";
                }
                else {
                    farewellMessage += u8" днів";
                }
            }

            if (farewellMessage.empty()) {
                farewellMessage = userName + u8" заблоковано";
            }

            bot.getApi().sendMessage(chatId, farewellMessage);
        }
        catch (TgBot::TgException& e) {
            bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
        }
        });

    bot.getEvents().onCommand("mute", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        int64_t chatId, userId;
        std::string userName;
        extractUserInfo(message, chatId, userId, userName);

        std::istringstream iss(message->text);
        std::string command, userTag, daysStr, farewellMessage;
        iss >> command >> userTag >> daysStr;
        std::getline(iss, farewellMessage);

        try {
            if (isAdminOrCreator(bot, chatId, userId)) {
                bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
                return;
            }

            int untilDate = 0;
            if (!daysStr.empty()) {
                try {
                    int days = std::stoi(daysStr);
                    time_t currentTime = std::time(0);
                    untilDate = static_cast<int>(currentTime) + days * 24 * 60 * 60;
                }
                catch (std::invalid_argument& e) {
                    bot.getApi().sendMessage(chatId, u8"Неправильна кількість днів");
                    return;
                }
            }

            bot.getApi().restrictChatMember(chatId, userId, TgBot::ChatPermissions::Ptr(new TgBot::ChatPermissions()), untilDate);

            if (farewellMessage.empty()) {
                farewellMessage = userName + u8" заборонено писати повідомлення";
                if (untilDate != 0) {
                    farewellMessage += u8" на " + daysStr;
                    if (daysStr == "1") {
                        farewellMessage += u8" день";
                    }
                    else if (daysStr == "2" || daysStr == "3" || daysStr == "4") {
                        farewellMessage += u8" дні";
                    }
                    else {
                        farewellMessage += u8" днів";
                    }
                }
                else {
                    farewellMessage += u8" назавжди";
                }
            }

            bot.getApi().sendMessage(chatId, farewellMessage);
        }
        catch (TgBot::TgException& e) {
            bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
        }
        });

    bot.getEvents().onCommand("unmute", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        int64_t chatId, userId;
        std::string userName;
        extractUserInfo(message, chatId, userId, userName);

        std::istringstream iss(message->text);
        std::string command, userTag;
        iss >> command >> userTag;

        try {
            if (isAdminOrCreator(bot, chatId, userId)) {
                bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
                return;
            }

            TgBot::ChatPermissions::Ptr permissions(new TgBot::ChatPermissions());
            permissions->canSendMessages = true;
            permissions->canSendOtherMessages = true;
            permissions->canSendPolls = true;
            permissions->canAddWebPagePreviews = true;
            permissions->canChangeInfo = true;
            permissions->canInviteUsers = true;
            permissions->canPinMessages = true;
            permissions->canManageTopics = true;
            bot.getApi().restrictChatMember(chatId, userId, permissions);

            std::string unmuteMessage = userName + u8" тепер може писати повідомлення";
            bot.getApi().sendMessage(chatId, unmuteMessage);
        }
        catch (TgBot::TgException& e) {
            bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
        }
        });

    bot.getEvents().onCommand("kick", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        int64_t chatId, userId;
        std::string userName;
        extractUserInfo(message, chatId, userId, userName);

        std::istringstream iss(message->text);
        std::string command, userTag;
        iss >> command >> userTag;

        try {
            if (isAdminOrCreator(bot, chatId, userId)) {
                bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
                return;
            }

            bot.getApi().banChatMember(chatId, userId);
            bot.getApi().unbanChatMember(chatId, userId);

            std::string kickMessage = userName + u8" було вигнано з чату";
            bot.getApi().sendMessage(chatId, kickMessage);
        }
        catch (TgBot::TgException& e) {
            bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
        }
        });

    bot.getEvents().onCommand("mutemedia", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        int64_t chatId, userId;
        std::string userName;
        extractUserInfo(message, chatId, userId, userName);

        std::istringstream iss(message->text);
        std::string command, userTag;
        iss >> command >> userTag;

        try {
            if (isAdminOrCreator(bot, chatId, userId)) {
                bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
                return;
            }

            TgBot::ChatPermissions::Ptr permissions(new TgBot::ChatPermissions());
            permissions->canSendAudios = false;
            permissions->canSendDocuments = false;
            permissions->canSendPhotos = false;
            permissions->canSendVideos = false;
            permissions->canSendVideoNotes = false;
            permissions->canSendVoiceNotes = false;
            permissions->canAddWebPagePreviews = false;
            permissions->canChangeInfo = true;
            permissions->canInviteUsers = true;
            permissions->canPinMessages = true;
            permissions->canManageTopics = true;
            permissions->canSendMessages = true;
            bot.getApi().restrictChatMember(chatId, userId, permissions);

            std::string nomedMessage = userName + u8" тепер не може надсилати медіа";
            bot.getApi().sendMessage(chatId, nomedMessage);
        }
        catch (TgBot::TgException& e) {
            bot.getApi().sendMessage(chatId, ERROR_MESSAGE);
        }
        });

    bot.getEvents().onCommand("startgame", [&bot](TgBot::Message::Ptr message) {
        handleGameStart(bot, message);
        });

    bot.getEvents().onCallbackQuery([&bot, &db](TgBot::CallbackQuery::Ptr query) {
        handleCallbackQuery(bot, query, db);
        });

    bot.getApi().deleteWebhook();

    try {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        TgBot::TgLongPoll longPoll(bot);
        while (true) {
            printf("Long poll started\n");
            longPoll.start();
        }
    }
    catch (TgBot::TgException& e) {
        printf("error: %s\n", e.what());
    }

    return 0;
}
