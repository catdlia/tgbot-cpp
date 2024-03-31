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

bool checkWin(const std::vector<std::vector<std::string>>& board) {

    for (int i = 0; i < 3; i++) {
        if (board[i][0] != " " && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
            return true;
        }
    }

    for (int i = 0; i < 3; i++) {
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

bool isBoardFull(const std::vector<std::vector<std::string>>& board) {
    for (const auto& row : board) {
        for (const auto& cell : row) {
            if (cell == " ") {
                return false;
            }
        }
    }
    return true;
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

void updateGameMessage(TgBot::Bot& bot, GameSession& session) {
    std::string messageText = u8"Гра у хрестики-нолики розпочалася!\n";
    messageText += u8"Гравець 1: " + session.player1Name + u8" (" + session.player1Symbol + ")\n";
    messageText += u8"Гравець 2: " + session.player2Name + u8" (" + session.player2Symbol + ")\n";
    if (checkWin(session.board))
        messageText += u8"Переможець: " + (session.currentPlayerId == session.player1Id ? session.player2Name : session.player1Name) + "\n";

    else if (isBoardFull(session.board)) {
        messageText += u8"Нічия!\n";
    }

    else {
    messageText += u8"Зараз хід гравця: " + (session.currentPlayerId == session.player1Id ? session.player1Name : session.player2Name) + "\n";
}

    TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup());
    for (int i = 0; i < 3; i++) {
        std::vector<TgBot::InlineKeyboardButton::Ptr> rowButtons;
        for (int j = 0; j < 3; j++) {
            TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton());
            button->text = session.board[i][j] == " " ? " " : session.board[i][j];
            button->callbackData = "gameId:" + std::to_string(session.gameId) + ":cell:" + std::to_string(i) + std::to_string(j);
            rowButtons.push_back(button);
        }
        keyboard->inlineKeyboard.push_back(rowButtons);
    }

    bot.getApi().editMessageText(messageText, session.chatId, session.messageId, "", "Markdown", false, keyboard);
}

std::map<int, GameSession> gameSessions;

void handleGameStart(TgBot::Bot& bot, TgBot::Message::Ptr message) {
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

    std::string gameIdPrefix = "gameId:";
    std::string cellPrefix = ":cell:";
    int gameId;

    try {
        gameId = std::stoi(query->data.substr(gameIdPrefix.size(), query->data.find(cellPrefix) - gameIdPrefix.size()));
    }
    catch (const std::invalid_argument& e) {
        bot.getApi().sendMessage(chatId, u8"Неправильний формат ID гри.");
        return;
    }
    catch (const std::out_of_range& e) {
        bot.getApi().sendMessage(chatId, u8"ID гри виходить за межі допустимого діапазону.");
        return;
    }

    if (query->data.rfind(gameIdPrefix, 0) == 0) {
        std::string gameIdStr = query->data.substr(gameIdPrefix.size());

        if (gameSessions.find(gameId) != gameSessions.end()) {
            GameSession& session = gameSessions[gameId];

            if (session.player2Id == 0) {
                session.player2Id = userId;
                session.player2Name = userName;

                std::srand(std::time(nullptr));
                session.player1Symbol = (std::rand() % 2) == 0 ? "X" : "O";
                session.player2Symbol = session.player1Symbol == "X" ? "O" : "X";

                session.currentPlayerId = (std::rand() % 2) == 0 ? session.player1Id : session.player2Id;

                updateGameMessage(bot, session);
            }
        }
        else {
            bot.getApi().answerCallbackQuery(query->id, u8"Гра з таким ID не знайдена.", false);
        }
    }

    if (query->data.find(cellPrefix) != std::string::npos) {
        std::string cellData = query->data.substr(query->data.find(cellPrefix) + cellPrefix.size());
        int row = cellData[0] - u8'0';
        int col = cellData[1] - u8'0';
        gameId = std::stoi(query->data.substr(gameIdPrefix.size(), query->data.find(cellPrefix) - gameIdPrefix.size()));

        if (gameSessions.find(gameId) != gameSessions.end()) {
            GameSession& session = gameSessions[gameId];

            if (userId != session.currentPlayerId) {
                bot.getApi().answerCallbackQuery(query->id, u8"Не лізь! Воно тебе зіжре!", false);
                return;
            }

            if (session.board[row][col] == " ") {
                session.board[row][col] = session.currentPlayerId == session.player1Id ? session.player1Symbol : session.player2Symbol;
                session.currentPlayerId = session.currentPlayerId == session.player1Id ? session.player2Id : session.player1Id;

                updateGameMessage(bot, session);

                if (checkWin(session.board)) {
                    try {
                        db.exec("BEGIN TRANSACTION");

                        int64_t WinnerId = session.currentPlayerId == session.player1Id ? session.player2Id : session.player1Id;

                        SQLite::Statement selectQuery(db, "SELECT wins, losses FROM player_ids WHERE id = ?");
                        selectQuery.bind(1, WinnerId);
                        int wins = 0, losses = 0;
                        if (selectQuery.executeStep()) {
                            wins = selectQuery.getColumn(0);
                            losses = selectQuery.getColumn(1);
                        }

                        SQLite::Statement insertOrReplaceQuery(db, "INSERT OR REPLACE INTO player_ids (id, wins, losses) VALUES (?, ?, ?)");
                        insertOrReplaceQuery.bind(1, WinnerId);
                        insertOrReplaceQuery.bind(2, wins + 1);
                        insertOrReplaceQuery.bind(3, losses);
                        insertOrReplaceQuery.exec();

                        db.exec("COMMIT");
                    }
                    catch (const SQLite::Exception& e) {
                        db.exec("ROLLBACK");
                        bot.getApi().sendMessage(session.chatId, u8"An error occurred while updating the database: " + std::string(e.what()));
                    }

                    try {
                        db.exec("BEGIN TRANSACTION");

                        SQLite::Statement selectQuery(db, "SELECT wins, losses FROM player_ids WHERE id = ?");
                        selectQuery.bind(1, session.currentPlayerId);
                        int wins = 0, losses = 0;
                        if (selectQuery.executeStep()) {
                            wins = selectQuery.getColumn(0);
                            losses = selectQuery.getColumn(1);
                        }

                        SQLite::Statement insertOrReplaceQuery(db, "INSERT OR REPLACE INTO player_ids (id, wins, losses) VALUES (?, ?, ?)");
                        insertOrReplaceQuery.bind(1, session.currentPlayerId);
                        insertOrReplaceQuery.bind(2, wins);
                        insertOrReplaceQuery.bind(3, losses + 1);
                        insertOrReplaceQuery.exec();

                        db.exec("COMMIT");
                    }
                    catch (const SQLite::Exception& e) {
                        db.exec("ROLLBACK");
                        bot.getApi().sendMessage(session.chatId, u8"An error occurred while updating the database: " + std::string(e.what()));
                    }

                    gameSessions.erase(gameId);
                }

                else if (isBoardFull(session.board)) {
                    gameSessions.erase(gameId);
                }
            }
            else {
                bot.getApi().answerCallbackQuery(query->id, u8"Ця клітинка вже зайнята!", false);
            }
        }
        else {
            bot.getApi().answerCallbackQuery(query->id, u8"Гра з таким ID не знайдена.", false);
        }
    }
}


int main() {
    SQLite::Database db("mydb.sqlite", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    db.exec("CREATE TABLE IF NOT EXISTS chat_ids (id INTEGER PRIMARY KEY, settings TEXT)");

    db.exec("CREATE TABLE IF NOT EXISTS player_ids (id INTEGER PRIMARY KEY, wins INTEGER, losses INTEGER, settings TEXT)");

    db.exec("PRAGMA busy_timeout = 100");

    std::vector<std::string> bot_commands = { "start", "test", "kick", "ban", "mute", "mutemedia", "unmute", "help", "startgame", "stats"}; // список команд бота

    TgBot::Bot bot("");

    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        bot.getApi().sendMessage(message->chat->id, u8"Привіт!");
        });

    bot.getEvents().onCommand("help", [&bot](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        bot.getApi().sendMessage(message->chat->id, u8"Список команд бота: \n\n/ban - блокує користувача.\n/mute - забороняє писати будь-які повідомлення.\n/unmute - знімає всі обмеження з користувача.\n/kick - виганяє людину з чату.\n/mutemedia - забороняє надсилати медіа.\n/startgame - починає гру в хрестики-нолики\n/stats - показує статистику ігор.");
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
        if (isMessageTooOld(message, 60)) return;

        handleGameStart(bot, message);
        });

    bot.getEvents().onCallbackQuery([&bot, &db](TgBot::CallbackQuery::Ptr query) {
        handleCallbackQuery(bot, query, db);
        });

    bot.getApi().deleteWebhook();

    bot.getEvents().onCommand("stats", [&bot, &db](TgBot::Message::Ptr message) {
        if (isMessageTooOld(message, 60)) return;

        int64_t userId = message->from->id;
        std::string userName = message->from->username;

        try {
            SQLite::Statement query(db, "SELECT wins, losses FROM player_ids WHERE id = ?");
            query.bind(1, userId);
            if (query.executeStep()) {
                int wins = query.getColumn(0);
                int losses = query.getColumn(1);
                int totalGames = wins + losses;

                std::stringstream statsMessage;
                statsMessage << userName << u8", ваша статистика:\n\n";
                statsMessage << u8"Перемоги: " << wins << "\n";
                statsMessage << u8"Поразки: " << losses << "\n";

                if (wins == 0) {
                    statsMessage << u8"Співвідношення перемог до поразок: Ви жодного разу не перемагали\n";
                }
                else if (losses == 0) {
                    statsMessage << u8"Співвідношення перемог до поразок: Ви жодного разу не програвали\n";
                }
                else {
                    double winLossRatio = static_cast<double>(wins) / static_cast<double>(losses);
                    statsMessage << u8"Співвідношення перемог до поразок: " << std::fixed << std::setprecision(2) << winLossRatio << "\n";
                }

                statsMessage << u8"Загальна кількість ігор: " << totalGames;

                bot.getApi().sendMessage(message->chat->id, statsMessage.str());
            }
            else {
                bot.getApi().sendMessage(message->chat->id, userName + u8", у вас ще немає статистики.");
            }
        }
        catch (const SQLite::Exception& e) {
            bot.getApi().sendMessage(message->chat->id, u8"Сталася помилка при отриманні статистики: " + std::string(e.what()));
        }
        });

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
