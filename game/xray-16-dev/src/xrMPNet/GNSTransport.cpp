#include "GNSTransport.h"

#include "NetMessageCodec.h"

#include <deque>
#include <exception>
#include <string>
#include <unordered_map>

#if XRMP_ENABLE_GNS
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#endif

namespace xrmp::net
{
#if !XRMP_ENABLE_GNS
struct GNSTransport::Impl
{
};

GNSTransport::GNSTransport() : impl_(std::make_unique<Impl>()) {}
GNSTransport::~GNSTransport() = default;

bool GNSTransport::listen(std::uint16_t, std::string* error)
{
    if (error)
        *error = "xrMPNet was built without XRMP_ENABLE_GNS";
    return false;
}

ConnectionId GNSTransport::connect(const TransportEndpoint&, std::string* error)
{
    if (error)
        *error = "xrMPNet was built without XRMP_ENABLE_GNS";
    return InvalidConnectionId;
}

void GNSTransport::disconnect(ConnectionId, DisconnectReason, std::string_view) {}
void GNSTransport::shutdown() {}
bool GNSTransport::send(ConnectionId, const NetMessage&, std::string* error)
{
    if (error)
        *error = "xrMPNet was built without XRMP_ENABLE_GNS";
    return false;
}
std::optional<TransportEvent> GNSTransport::poll() { return std::nullopt; }
TransportMetrics GNSTransport::metrics(ConnectionId) const { return {}; }
#else
struct GNSTransport::Impl
{
    ISteamNetworkingSockets* sockets = nullptr;
    HSteamListenSocket listenSocket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup pollGroup = k_HSteamNetPollGroup_Invalid;
    std::deque<TransportEvent> pending;
    std::unordered_map<ConnectionId, TransportMetrics> metrics;
};

namespace
{
GNSTransport::Impl* g_activeTransport = nullptr;

int deliveryFlags(Channel channel)
{
    switch (channel)
    {
    case Channel::ReliableOrdered: return k_nSteamNetworkingSend_Reliable;
    case Channel::ReliableUnordered: return k_nSteamNetworkingSend_ReliableNoNagle;
    case Channel::UnreliableSequenced: return k_nSteamNetworkingSend_UnreliableNoNagle;
    }
    return k_nSteamNetworkingSend_Reliable;
}

ConnectionId toPublicId(HSteamNetConnection connection)
{
    return static_cast<ConnectionId>(connection);
}

HSteamNetConnection toGnsId(ConnectionId connection)
{
    return static_cast<HSteamNetConnection>(connection);
}

DisconnectReason mapEndReason(int endReason)
{
    if (endReason == k_ESteamNetConnectionEnd_AppException_Generic)
        return DisconnectReason::TransportError;
    if (endReason >= k_ESteamNetConnectionEnd_App_Min && endReason <= k_ESteamNetConnectionEnd_App_Max)
        return DisconnectReason::RemoteClosed;
    return DisconnectReason::Timeout;
}

void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
{
    if (!g_activeTransport || !g_activeTransport->sockets)
        return;

    const auto connection = toPublicId(info->m_hConn);
    switch (info->m_info.m_eState)
    {
    case k_ESteamNetworkingConnectionState_Connecting:
        g_activeTransport->sockets->AcceptConnection(info->m_hConn);
        if (g_activeTransport->pollGroup != k_HSteamNetPollGroup_Invalid)
            g_activeTransport->sockets->SetConnectionPollGroup(info->m_hConn, g_activeTransport->pollGroup);
        break;
    case k_ESteamNetworkingConnectionState_Connected:
        g_activeTransport->pending.push_back(
            TransportEvent{ TransportEvent::Kind::Connected, connection, DisconnectReason::RemoteClosed, {}, {} });
        break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        g_activeTransport->pending.push_back(TransportEvent{ TransportEvent::Kind::Disconnected, connection,
            mapEndReason(info->m_info.m_eEndReason), info->m_info.m_szEndDebug, {} });
        g_activeTransport->metrics.erase(connection);
        g_activeTransport->sockets->CloseConnection(info->m_hConn, 0, nullptr, false);
        break;
    default: break;
    }
}
} // namespace

GNSTransport::GNSTransport() : impl_(std::make_unique<Impl>())
{
    SteamDatagramErrMsg error{};
    if (!GameNetworkingSockets_Init(nullptr, error))
        return;

    impl_->sockets = SteamNetworkingSockets();
    if (impl_->sockets)
        impl_->pollGroup = impl_->sockets->CreatePollGroup();

    g_activeTransport = impl_.get();
    if (SteamNetworkingUtils())
        SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(onConnectionStatusChanged);
}

GNSTransport::~GNSTransport()
{
    shutdown();
    GameNetworkingSockets_Kill();
}

bool GNSTransport::listen(std::uint16_t port, std::string* error)
{
    if (!impl_->sockets)
    {
        if (error)
            *error = "GameNetworkingSockets is not initialized";
        return false;
    }

    SteamNetworkingIPAddr address;
    address.Clear();
    address.m_port = port;

    impl_->listenSocket = impl_->sockets->CreateListenSocketIP(address, 0, nullptr);
    if (impl_->listenSocket == k_HSteamListenSocket_Invalid)
    {
        if (error)
            *error = "CreateListenSocketIP failed";
        return false;
    }

    return true;
}

ConnectionId GNSTransport::connect(const TransportEndpoint& endpoint, std::string* error)
{
    if (!impl_->sockets)
    {
        if (error)
            *error = "GameNetworkingSockets is not initialized";
        return InvalidConnectionId;
    }

    SteamNetworkingIPAddr address;
    if (!address.ParseString((endpoint.host + ":" + std::to_string(endpoint.port)).c_str()))
    {
        if (error)
            *error = "invalid endpoint";
        return InvalidConnectionId;
    }

    const auto connection = impl_->sockets->ConnectByIPAddress(address, 0, nullptr);
    if (connection == k_HSteamNetConnection_Invalid)
    {
        if (error)
            *error = "ConnectByIPAddress failed";
        return InvalidConnectionId;
    }

    if (impl_->pollGroup != k_HSteamNetPollGroup_Invalid)
        impl_->sockets->SetConnectionPollGroup(connection, impl_->pollGroup);

    return toPublicId(connection);
}

void GNSTransport::disconnect(ConnectionId connection, DisconnectReason reason, std::string_view diagnostic)
{
    if (!impl_->sockets || connection == InvalidConnectionId)
        return;

    impl_->sockets->CloseConnection(toGnsId(connection), static_cast<int>(reason), diagnostic.data(), false);
    impl_->metrics.erase(connection);
}

void GNSTransport::shutdown()
{
    if (!impl_ || !impl_->sockets)
        return;

    if (g_activeTransport == impl_.get())
    {
        if (SteamNetworkingUtils())
            SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(nullptr);
        g_activeTransport = nullptr;
    }

    if (impl_->listenSocket != k_HSteamListenSocket_Invalid)
    {
        impl_->sockets->CloseListenSocket(impl_->listenSocket);
        impl_->listenSocket = k_HSteamListenSocket_Invalid;
    }

    if (impl_->pollGroup != k_HSteamNetPollGroup_Invalid)
    {
        impl_->sockets->DestroyPollGroup(impl_->pollGroup);
        impl_->pollGroup = k_HSteamNetPollGroup_Invalid;
    }

    impl_->pending.clear();
    impl_->metrics.clear();
    impl_->sockets = nullptr;
}

bool GNSTransport::send(ConnectionId connection, const NetMessage& message, std::string* error)
{
    if (!impl_->sockets || connection == InvalidConnectionId)
    {
        if (error)
            *error = "invalid transport connection";
        return false;
    }

    Bytes wire;
    try
    {
        wire = serializeMessage(message);
    }
    catch (const std::exception& e)
    {
        if (error)
            *error = e.what();
        return false;
    }

    const auto result = impl_->sockets->SendMessageToConnection(
        toGnsId(connection), wire.data(), static_cast<std::uint32_t>(wire.size()), deliveryFlags(message.channel), nullptr);
    if (result != k_EResultOK)
    {
        if (error)
            *error = "SendMessageToConnection failed";
        return false;
    }

    auto& metrics = impl_->metrics[connection];
    metrics.bytesSent += wire.size();
    ++metrics.packetsSent;
    return true;
}

std::optional<TransportEvent> GNSTransport::poll()
{
    if (!impl_->pending.empty())
    {
        TransportEvent event = std::move(impl_->pending.front());
        impl_->pending.pop_front();
        return event;
    }

    if (!impl_->sockets || impl_->pollGroup == k_HSteamNetPollGroup_Invalid)
        return std::nullopt;

    ISteamNetworkingMessage* messages[16]{};
    const int count = impl_->sockets->ReceiveMessagesOnPollGroup(impl_->pollGroup, messages, 16);
    for (int i = 0; i < count; ++i)
    {
        ISteamNetworkingMessage* raw = messages[i];
        const auto connection = toPublicId(raw->m_conn);
        TransportEvent event;
        event.kind = TransportEvent::Kind::Message;
        event.connection = connection;

        try
        {
            const auto* begin = static_cast<const Byte*>(raw->m_pData);
            const Bytes bytes(begin, begin + raw->m_cbSize);
            event.message = deserializeMessage(bytes);
            auto& metrics = impl_->metrics[connection];
            metrics.bytesReceived += bytes.size();
            ++metrics.packetsReceived;
        }
        catch (const std::exception& e)
        {
            event.kind = TransportEvent::Kind::Disconnected;
            event.reason = DisconnectReason::ProtocolMismatch;
            event.diagnostic = e.what();
        }

        impl_->pending.push_back(std::move(event));
        raw->Release();
    }

    impl_->sockets->RunCallbacks();

    if (!impl_->pending.empty())
    {
        TransportEvent event = std::move(impl_->pending.front());
        impl_->pending.pop_front();
        return event;
    }

    return std::nullopt;
}

TransportMetrics GNSTransport::metrics(ConnectionId connection) const
{
    const auto found = impl_->metrics.find(connection);
    if (found == impl_->metrics.end())
        return {};

    TransportMetrics out = found->second;
    SteamNetConnectionRealTimeStatus_t status{};
    if (impl_->sockets &&
        impl_->sockets->GetConnectionRealTimeStatus(toGnsId(connection), &status, 0, nullptr) == k_EResultOK)
    {
        out.rttMs = static_cast<std::uint32_t>(status.m_nPing);
        out.packetLoss = status.m_flConnectionQualityLocal < 0.0f ? 0.0f : 1.0f - status.m_flConnectionQualityLocal;
    }
    return out;
}
#endif
} // namespace xrmp::net
