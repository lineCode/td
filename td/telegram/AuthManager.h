//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2018
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/net/NetActor.h"
#include "td/telegram/net/NetQuery.h"

#include "td/telegram/td_api.h"
#include "td/telegram/telegram_api.h"

#include "td/utils/common.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

namespace td {
class SendCodeHelper {
 public:
  void on_sent_code(telegram_api::object_ptr<telegram_api::auth_sentCode> sent_code);
  td_api::object_ptr<td_api::authorizationStateWaitCode> get_authorization_state_wait_code() const;
  td_api::object_ptr<td_api::authenticationCodeInfo> get_authentication_code_info_object() const;
  Result<telegram_api::auth_resendCode> resend_code();
  Result<telegram_api::auth_sendCode> send_code(Slice phone_number, bool allow_flash_call, bool is_current_phone_number,
                                                int32 api_id, const string &api_hash);
  Result<telegram_api::account_sendChangePhoneCode> send_change_phone_code(Slice phone_number, bool allow_flash_call,
                                                                           bool is_current_phone_number);

  Slice phone_number() const {
    return phone_number_;
  }
  Slice phone_code_hash() const {
    return phone_code_hash_;
  }
  bool phone_registered() const {
    return phone_registered_;
  }

 private:
  static constexpr int32 AUTH_SEND_CODE_FLAG_ALLOW_FLASH_CALL = 1 << 0;

  static constexpr int32 SENT_CODE_FLAG_IS_USER_REGISTERED = 1 << 0;
  static constexpr int32 SENT_CODE_FLAG_HAS_NEXT_TYPE = 1 << 1;
  static constexpr int32 SENT_CODE_FLAG_HAS_TIMEOUT = 1 << 2;

  struct AuthenticationCodeInfo {
    enum class Type { None, Message, Sms, Call, FlashCall };
    Type type = Type::None;
    int32 length = 0;
    string pattern;

    AuthenticationCodeInfo() = default;
    AuthenticationCodeInfo(Type type, int length, string pattern)
        : type(type), length(length), pattern(std::move(pattern)) {
    }
  };

  string phone_number_;
  bool phone_registered_;
  string phone_code_hash_;

  SendCodeHelper::AuthenticationCodeInfo sent_code_info_;
  SendCodeHelper::AuthenticationCodeInfo next_code_info_;
  int32 next_code_timeout_;

  static AuthenticationCodeInfo get_authentication_code_info(
      tl_object_ptr<telegram_api::auth_CodeType> &&code_type_ptr);
  static AuthenticationCodeInfo get_authentication_code_info(
      tl_object_ptr<telegram_api::auth_SentCodeType> &&sent_code_type_ptr);

  static tl_object_ptr<td_api::AuthenticationCodeType> get_authentication_code_type_object(
      const AuthenticationCodeInfo &authentication_code_info);
};

class ChangePhoneNumberManager : public NetActor {
 public:
  explicit ChangePhoneNumberManager(ActorShared<> parent);
  void get_state(uint64 query_id);

  void change_phone_number(uint64 query_id, string phone_number, bool allow_flash_call, bool is_current_phone_number);
  void resend_authentication_code(uint64 query_id);
  void check_code(uint64 query_id, string code);

 private:
  enum class State { Ok, WaitCode } state_ = State::Ok;
  enum class NetQueryType { None, SendCode, ChangePhone };

  ActorShared<> parent_;
  uint64 query_id_ = 0;
  uint64 net_query_id_ = 0;
  NetQueryType net_query_type_;

  SendCodeHelper send_code_helper_;

  void on_new_query(uint64 query_id);
  void on_query_error(Status status);
  void on_query_error(uint64 id, Status status);
  void on_query_ok();
  void start_net_query(NetQueryType net_query_type, NetQueryPtr net_query);

  void on_change_phone_result(NetQueryPtr &result);
  void on_send_code_result(NetQueryPtr &result);
  void on_result(NetQueryPtr result) override;
  void tear_down() override;
};

class AuthManager : public NetActor {
 public:
  AuthManager(int32 api_id, const string &api_hash, ActorShared<> parent);

  bool is_bot() const;

  bool is_authorized() const;
  void get_state(uint64 query_id);

  void set_phone_number(uint64 query_id, string phone_number, bool allow_flash_call, bool is_current_phone_number);
  void resend_authentication_code(uint64 query_id);
  void check_code(uint64 query_id, string code, string first_name, string last_name);
  void check_bot_token(uint64 query_id, string bot_token);
  void check_password(uint64 query_id, string password);
  void request_password_recovery(uint64 query_id);
  void recover_password(uint64 query_id, string code);
  void logout(uint64 query_id);
  void delete_account(uint64 query_id, const string &reason);

  void on_closing();

 private:
  static constexpr size_t MAX_NAME_LENGTH = 255;  // server side limit

  enum class State { None, WaitPhoneNumber, WaitCode, WaitPassword, Ok, LoggingOut, Closing } state_ = State::None;
  enum class NetQueryType {
    None,
    SignIn,
    SignUp,
    SendCode,
    GetPassword,
    CheckPassword,
    RequestPasswordRecovery,
    RecoverPassword,
    BotAuthentication,
    Authentication,
    LogOut,
    DeleteAccount
  };

  ActorShared<> parent_;

  int32 api_id_;
  string api_hash_;

  SendCodeHelper send_code_helper_;

  string bot_token_;
  uint64 query_id_ = 0;

  string current_salt_;
  string new_salt_;
  string hint_;
  bool has_recovery_;
  string email_address_pattern_;

  bool was_check_bot_token_ = false;
  bool is_bot_ = false;
  uint64 net_query_id_ = 0;
  NetQueryType net_query_type_;

  void on_new_query(uint64 query_id);
  void on_query_error(Status status);
  void on_query_error(uint64 id, Status status);
  void on_query_ok();
  void start_net_query(NetQueryType net_query_type, NetQueryPtr net_query);

  void on_send_code_result(NetQueryPtr &result);
  void on_get_password_result(NetQueryPtr &result);
  void on_request_password_recovery_result(NetQueryPtr &result);
  void on_authentication_result(NetQueryPtr &result, bool expected_flag);
  void on_log_out_result(NetQueryPtr &result);
  void on_delete_account_result(NetQueryPtr &result);
  void on_authorization(tl_object_ptr<telegram_api::auth_authorization> auth);

  void on_result(NetQueryPtr result) override;

  void update_state(State new_state, bool force = false);
  tl_object_ptr<td_api::AuthorizationState> get_authorization_state_object(State authorization_state) const;
  void send_ok(uint64 query_id);

  void start_up() override;
  void tear_down() override;
};
}  // namespace td
