#include "core/message.h"

#include <algorithm>

namespace zibby::core {

MessageService::MessageService(Database& database, const Crypto& crypto, std::string storageSecret)
	: database_(database)
	, crypto_(crypto)
	, storageSecret_(std::move(storageSecret)) {}

std::optional<std::int64_t> MessageService::sendText(const std::string& chatId, const std::string& from, const std::string& to, const std::string& text) {
	if (!validateText(text) || chatId.empty() || from.empty() || to.empty()) {
		return std::nullopt;
	}

	Message message;
	message.chatId = chatId;
	message.from = from;
	message.to = to;
	message.payload = crypto_.encryptForStorage(text, storageSecret_);
	if (message.payload.empty()) {
		return std::nullopt;
	}

	std::int64_t messageId = 0;
	if (!database_.insertMessage(message, &messageId)) {
		return std::nullopt;
	}

	return messageId;
}

bool MessageService::editMessage(std::int64_t messageId, const std::string& newText) {
	if (messageId <= 0 || !validateText(newText)) {
		return false;
	}

	const std::string encrypted = crypto_.encryptForStorage(newText, storageSecret_);
	if (encrypted.empty()) {
		return false;
	}

	return database_.updateMessagePayload(messageId, encrypted);
}

bool MessageService::markRead(std::int64_t messageId) {
	if (messageId <= 0) {
		return false;
	}
	return database_.markMessageRead(messageId);
}

std::vector<Message> MessageService::history(const std::string& chatId, std::size_t limit) const {
	auto records = database_.getMessages(chatId, limit);
	for (auto& record : records) {
		const std::string decrypted = crypto_.decryptFromStorage(record.payload, storageSecret_);
		if (!decrypted.empty()) {
			record.payload = decrypted;
		}
	}
	return records;
}

bool MessageService::validateText(const std::string& text) {
	if (text.empty() || text.size() > 4000) {
		return false;
	}

	const auto firstNotSpace = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c) != 0; });
	return firstNotSpace != text.end();
}

} // namespace zibby::core
