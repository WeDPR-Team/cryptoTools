#pragma once
#include "cryptoTools/Common/config.h"

#ifdef ENABLE_SSL

#include <string>
#include <boost/system/error_code.hpp>
#include <boost/asio/strand.hpp>
#include <cryptoTools/Network/SocketAdapter.h>
#include <cryptoTools/Common/Log.h>
#include <memory>


#ifdef ENABLE_OPENSSL

#include <openssl/ssl.h>

#else // ENABLE_WOLFSSL

#ifndef WC_NO_HARDEN
#define WC_NO_HARDEN
#endif

#ifdef _MSC_VER
#define WOLFSSL_USER_SETTINGS
#define WOLFSSL_LIB
#endif

#include <wolfssl/ssl.h>
#if defined(_MSC_VER) && !defined(KEEP_PEER_CERT)
    #error "please compile wolfSSl with KEEP_PEER_CERT. add this to the user_setting.h file in wolfssl..."
#endif

#endif

#ifdef ENABLE_NET_LOG
#define WOLFSSL_LOGGING
#endif

namespace osuCrypto
{
    using error_code = boost::system::error_code;
    error_code readFile(const std::string& file, std::vector<u8>& buffer);

    enum class OpenSSL_errc
    {
        Success = 0,
        Failure = 1
    };
    enum class TLS_errc
    {
        Success = 0,
        Failure,
        ContextNotInit,
        ContextAlreadyInit,
        ContextFailedToInit,
        OnlyValidForServerContext,
        SessionIDMismatch
    };
}

namespace boost {
    namespace system {
        template <>
        struct is_error_code_enum<osuCrypto::OpenSSL_errc> : true_type {};
        template <>
        struct is_error_code_enum<osuCrypto::TLS_errc> : true_type {};
    }
}

namespace { // anonymous namespace

    struct OpenSSLErrCategory : boost::system::error_category
    {
        const char* name() const noexcept override
        {
            return "osuCrypto_OpenSSL";
        }

        std::string message(int err) const override
        {
            if (err == 0) return "Success";
            if (err == 1) return "Failure";

            std::terminate();
            //std::array<char, WOLFSSL_MAX_ERROR_SZ> buffer;
            //return wolfSSL_ERR_error_string(err, buffer.data());
        }
    };

    const OpenSSLErrCategory WolfSSLCategory{};


    struct TLSErrCategory : boost::system::error_category
    {
        const char* name() const noexcept override
        {
            return "osuCrypto_TLS";
        }

        std::string message(int err) const override
        {
            switch (static_cast<osuCrypto::TLS_errc>(err))
            {
            case osuCrypto::TLS_errc::Success:
                return "Success";
            case osuCrypto::TLS_errc::Failure:
                return "Generic Failure";
            case osuCrypto::TLS_errc::ContextNotInit:
                return "TLS context not init";
            case osuCrypto::TLS_errc::ContextAlreadyInit:
                return "TLS context is already init";
            case osuCrypto::TLS_errc::ContextFailedToInit:
                return "TLS context failed to init";
            case osuCrypto::TLS_errc::OnlyValidForServerContext:
                return "Operation is only valid for server initialized TLC context";
            case osuCrypto::TLS_errc::SessionIDMismatch:
                return "Critical error on connect. Likely active attack by thirdparty";
            default:
                return "unknown error";
            }
        }
    };

    const TLSErrCategory TLSCategory{};

} // anonymous namespace

namespace osuCrypto
{
    inline error_code make_error_code(OpenSSL_errc e)
    {
        auto ee = static_cast<int>(e);
        return { ee, OpenSSLCategory };
    }

    inline error_code make_error_code(TLS_errc e)
    {
        auto ee = static_cast<int>(e);
        return { ee, TLSCategory };
    }


    inline error_code ssl_error_code(int ret)
    {
        const auto OPENSSL_SUCCESS = 0;
        const auto OPENSSL_FAILURE = 1;
        switch (ret)
        {
        case OPENSSL_SUCCESS: return make_error_code(OpenSSL_errc::Success);
        case OPENSSL_FAILURE: return make_error_code(OpenSSL_errc::Failure);
        default: return make_error_code(OpenSSL_errc(ret));
        }
    }

    struct OpenSSLContext
    {
        enum class Mode
        {
            Client,
            Server,
            Both
        };

        struct Base
        {
            SSL_METHOD* mMethod = nullptr;
            SSL_CTX* mCtx = nullptr;
            Mode mMode = Mode::Client;

            Base(Mode mode);
            ~Base();
        };

        std::shared_ptr<Base> mBase;


        void init(Mode mode, error_code& ec);

        void loadCertFile(std::string path, error_code& ec);
        void loadCert(span<u8> data, error_code& ec);

        void loadKeyPairFile(std::string pkPath, std::string skPath, error_code& ec);
        void loadKeyPair(span<u8> pkData, span<u8> skData, error_code& ec);

        void requestClientCert(error_code& ec);


        bool isInit() const {
            return mBase != nullptr;                
        }
        Mode mode() const {
            if (isInit())
                return mBase->mMode;
            else return Mode::Both;
        }

        operator bool() const
        {
            return isInit();
        }


        operator SSL_CTX* () const
        {
            return mBase ? mBase->mCtx : nullptr;
        }

    };

    using TLSContext = OpenSSLContext;


    struct OpenSSLCertX509
    {
        SSL_X509* mCert = nullptr;

        std::string commonName()
        {
            std::terminate();
            //return wolfSSL_X509_get_subjectCN(mCert);
        }

        std::string notAfter()
        {
            std::terminate();
            //WOLFSSL_ASN1_TIME* ptr = wolfSSL_X509_get_notAfter(mCert);
            //std::array<char, 1024> buffer;
            //wolfSSL_ASN1_TIME_to_string(ptr, buffer.data(), buffer.size());
            //return buffer.data();
        }


        std::string notBefore()
        {
            std::terminate();
            //WOLFSSL_ASN1_TIME* ptr = wolfSSL_X509_get_notBefore(mCert);
            //std::array<char, 1024> buffer;
            //wolfSSL_ASN1_TIME_to_string(ptr, buffer.data(), buffer.size());
            //return buffer.data();
        }
    };

    struct OpenSSLSocket : public SocketInterface, public LogAdapter
    {

        using buffer = boost::asio::mutable_buffer;

        boost::asio::ip::tcp::socket mSock;
        boost::asio::strand<boost::asio::io_context::executor_type> mStrand;
        boost::asio::io_context& mIos;
        SSL* mSSL = nullptr;
#ifdef WOLFSSL_LOGGING
        oc::Log mLog_;
#endif
        std::vector<buffer> mSendBufs, mRecvBufs;

        u64 mSendBufIdx = 0, mRecvBufIdx = 0;
        u64 mSendBT = 0, mRecvBT = 0;

        error_code mSendEC, mRecvEC, mSetupEC;

        io_completion_handle mSendCB, mRecvCB;
        completion_handle mSetupCB, mShutdownCB;

        bool mCancelingPending = false;

        struct State
        {
            enum class Phase { Uninit, Connect, Accept, Normal, Closed };
            Phase mPhase = Phase::Uninit;
            span<char> mPendingSendBuf;
            span<char> mPendingRecvBuf;
            bool hasPendingSend() { return mPendingSendBuf.size() > 0; }
            bool hasPendingRecv() { return mPendingRecvBuf.size() > 0; }
        };

        State mState;

        OpenSSLSocket(boost::asio::io_context& ios, OpenSSLContext& ctx);
        OpenSSLSocket(boost::asio::io_context& ios, boost::asio::ip::tcp::socket&& sock, WolfContext& ctx);

        OpenSSLSocket(OpenSSLSocket&&) = delete;
        OpenSSLSocket(const OpenSSLSocket&) = delete;

        ~OpenSSLSocket()
        {
            close();
            if (mSSL) SSL_free(mSSL);
        }

        void close() override;

        void cancel() override;
        
        void async_send(
            span<buffer> buffers,
            io_completion_handle&& fn) override;

        void async_recv(
            span<buffer> buffers,
            io_completion_handle&& fn) override;



        void setDHParamFile(std::string path, error_code& ec);
        void setDHParam(span<u8> paramData, error_code& ec);

        OpenSSLCertX509 getCert();

        bool hasRecvBuffer() { return mRecvBufIdx < mRecvBufs.size(); }
        buffer& curRecvBuffer() { return mRecvBufs[mRecvBufIdx]; }

        bool hasSendBuffer() { return mSendBufIdx < mSendBufs.size(); }
        buffer& curSendBuffer() { return mSendBufs[mSendBufIdx]; }

        void send(
            span<buffer> buffers,
            error_code& ec,
            u64& bt);

        void sendNext();

        int sslRquestSendCB(char* buf, int size);

        void recv(
            span<buffer> buffers,
            error_code& ec,
            u64& bt);
        

        void recvNext();

        int sslRquestRecvCB(char* buf, int size);


        void connect(error_code& ec);
        void async_connect(completion_handle&& cb) override;
        void connectNext();

        void accept(error_code& ec);
        void async_accept(completion_handle&& cb) override;
        void acceptNext();

#ifdef WOLFSSL_LOGGING
        void LOG(std::string X);
#endif

        static int recvCallback(SSL* ssl, char* buf, int size, void* ctx)
        {
            //lout << "in recv cb with " << std::hex << u64(ctx) << std::endl;
            OpenSSLSocket& sock = *(OpenSSLSocket*)ctx;
            assert(sock.mSSL == ssl);
            return sock.sslRquestRecvCB(buf, size);
        }

        static int sendCallback(SSL* ssl, char* buf, int size, void* ctx)
        {
            //lout << "in send cb with " << std::hex << u64(ctx) << std::endl;
            OpenSSLSocket& sock = *(OpenSSLSocket*)ctx;
            assert(sock.mSSL == ssl);
            return sock.sslRquestSendCB(buf, size);
        }
    };

    using TLSSocket = OpenSSLSocket;

    extern std::array<u8, 5010> sample_ca_cert_pem;
    extern std::array<u8, 0x26ef> sample_server_cert_pem;
    extern std::array<u8, 0x68f> sample_server_key_pem;
    extern std::array<u8, 0x594> sample_dh2048_pem;

}

#else
namespace osuCrypto
{
    struct TLSContext {
        operator bool() const
        {
            return false;
        }
};
}
#endif