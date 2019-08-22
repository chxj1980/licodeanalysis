
#include "StormWebrtc/StormWebrtc.h"
#include "StormWebrtcServerAPI/StormWebrtcServer.h"

int main(int argc, char* argv[])
{
  StormWebrtcStaticInit();

  StormWebrtcServerSettings settings;
  settings.m_MaxConnections = 256;
  settings.m_Port = 61200;
  settings.m_KeyFile = "localhost.key";
  settings.m_CertFile = "localhost.crt";
  settings.m_InStreams.push_back(StormWebrtcStreamType::kReliable);
  settings.m_OutStreams.push_back(StormWebrtcStreamType::kReliable);

  auto server = CreateStormWebrtcServer(settings);

  StormWebrtcEvent ev;

  while (true)
  {
    server->Update();
    while (server->PollEvent(ev))
    {
      switch (ev.m_Type)
      {
      case StormWebrtcEventType::kConnected:
        printf("Connected\n");
        break;
      case StormWebrtcEventType::kDisconnected:
        printf("Disconnected\n");
        break;
      case StormWebrtcEventType::kData:
        printf("Data\n");
        server->SendPacket(ev.m_ConnectionHandle, "craps", 5, 0, true);
        break;
      }
    }
  }

  StormWebrtcStaticCleanup();
  return 0;
}


