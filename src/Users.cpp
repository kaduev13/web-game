#include "WebgameServer.hpp"
#include "Router.hpp"
#include "DBConnector.hpp"
#include "User.hpp"

#include "Poco/JSON/Object.h"
#include "Poco/JSON/Query.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Stringifier.h"

#include "Poco/Net/HTMLForm.h"

#include "Poco/Random.h"
#include "Poco/PBKDF2Engine.h"
#include "Poco/HMACEngine.h"
#include "Poco/SHA1Engine.h"

User::User(std::string field, std::string value) {
    auto user = DBConnection::instance().ExecParams("SELECT id, name FROM users WHERE " + field + "=$1", { value });
    if (user.row_count() != 1) throw std::exception();
    id = stoi((*user.begin()).field_by_name("id"));
    name = (*user.begin()).field_by_name("name");
}

#include <iomanip>

namespace {
    Poco::Random rnd;

    std::string genRandomString(int len = 64) {
        std::stringstream res;
        for (int i = 0; i < (int)ceil(len / 8); i++)
            res << std::setfill('0') << std::setw(8) << std::hex << rnd.next();
        return res.str().substr(0, len);
    }

    class JsonException : public std::runtime_error
    {
    public:
        JsonException(std::string message) : runtime_error(message) {}
    };

    class JsonRequest {
        Poco::Dynamic::Var v;
        const RouteMatch& _m;
    public:
        JsonRequest(const RouteMatch& m) : _m(m) {
            Poco::Net::HTMLForm form(m.request(), m.request().stream());
            Poco::JSON::Parser p;
            v = p.parse(form.get("jsonObj"));
        }

        template <typename T>
        void get(const std::string & name, T & value) const {
            Poco::JSON::Query q(v);
            try {
                q.find(name).convert(value);
            }
            catch (...) {
                throw JsonException("BadRequest");
            }
        }
    };

    class JsonResponse {
        const RouteMatch & _m;
        Poco::JSON::Object data;
    public:
        JsonResponse(const RouteMatch& m) : _m(m) {}

        template <typename T>
        void set(const std::string & name, T & value) {
            data.set(name, value);
        }

        void send(const std::string result) {
            Poco::JSON::Object response;
            response.set("result", result);
            response.set("data", (data.size() != 0) ? data : "null");
            std::ostream& st = _m.response().send();
            Poco::JSON::Stringifier::condense(response, st);
            st.flush();
        }
    };

#define SALT_LENGTH 64

    std::string getPasswordHash(std::string password, std::string salt = genRandomString(SALT_LENGTH)) {
        Poco::PBKDF2Engine<Poco::HMACEngine<Poco::SHA1Engine>> hs(salt, 4096);
        hs.update(password);
        return salt + ":" + hs.digestToHex(hs.digest());
    }

    void Login(JsonRequest & rq, JsonResponse & rs) {
        std::string login, password;
        rq.get("login", login);
        rq.get("password", password);

        auto user = DBConnection::instance().ExecParams("SELECT password, name FROM users WHERE login=$1", { login });
        if (user.row_count() != 1) return rs.send("BadCredentials");
        std::string passhash = (*user.begin()).field_by_name("password");
        if (getPasswordHash(password, passhash.substr(0, SALT_LENGTH)).compare(passhash))
            return rs.send("BadCredentials");
        std::string token = genRandomString();
        DBConnection::instance().ExecParams("UPDATE users SET token = $1 WHERE login = $2", { token, login });
        std::string name = (*user.begin()).field_by_name("name");
        rs.set("name", name);
        rs.set("accessToken", token);
        rs.send("Ok");
    }

    void Logout(JsonRequest & rq, JsonResponse & rs) {
        std::string token;
        rq.get("accessToken", token);

        auto res = DBConnection::instance().ExecParams("SELECT token FROM users WHERE token=$1", { token });
        if (!res.row_count()) return rs.send("NotLoggedIn");
        DBConnection::instance().ExecParams("UPDATE users SET token = '' WHERE token = $1", { token });
        rs.send("Ok");
    }

    void Register(JsonRequest & rq, JsonResponse & rs) {
        std::string login, password, name;
        rq.get("login", login);
        rq.get("password", password);
        rq.get("name", name);

        if (login.length() < 2 || login.length() > 36) return rs.send("BadLogin");
        if (DBConnection::instance().ExecParams("SELECT login FROM users WHERE login=$1", { login }).row_count() != 0) return rs.send("LoginExists");
        if (password.length() < 2 || password.length() > 36) return rs.send("BadPassword");
        if (name.length() < 2 || name.length() > 36) return rs.send("BadName");

        std::string token = genRandomString();
        DBConnection::instance().ExecParams("INSERT INTO users (login, name, password, token) values ($1, $2, $3, $4)", {
            login, name, getPasswordHash(password), token
        });

        rs.set("accessToken", token);
        rs.send("Ok");
    }

    template <void(*T)(JsonRequest &, JsonResponse &)>
    void CatchException(const RouteMatch& m) {
        JsonRequest rq(m);
        JsonResponse rs(m);
        try {
            T(rq, rs);
        }
        catch (const JsonException & e) {
            rs.send(e.what());
        }
        catch (const std::exception & e) {
            if (m.response().sent()) throw e;
            else rs.send("InternalError");
        }
    }

    class Pages {
    public:
        Pages() {
            rnd.seed();
            auto & router = Router::instance();
            router.registerRoute("POST", "/api/login", CatchException<Login>);
            router.registerRoute("POST", "/api/register", CatchException<Register>);
            router.registerRoute("POST", "/api/logout", CatchException<Logout>);
        }
    };

    Pages pages;
}
