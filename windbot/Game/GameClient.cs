﻿using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using YGOSharp.Network;
using YGOSharp.Network.Enums;
using YGOSharp.Network.Utils;

namespace WindBot.Game
{
    public class GameClient
    {
        public YGOClient Connection { get; private set; }
        public string Username;
        public string Deck;
        public string DeckFile;
        public string Dialog;
        public int Hand;
        public bool Debug;
        public bool _chat;
        private string _serverHost;
        private int _serverPort;
        private short _proVersion;

        private string _roomInfo;

        private GameBehavior _behavior;

        public GameClient(WindBotInfo Info)
        {
            Username = Info.Name;
            Deck = Info.Deck;
            DeckFile = Info.DeckFile;
            Dialog = Info.Dialog;
            Hand = Info.Hand;
            Debug = Info.Debug;
            _chat = Info.Chat;
            _serverHost = Info.Host;
            _serverPort = Info.Port;
            _roomInfo = Info.HostInfo;
            _proVersion = (short)Info.Version;
        }

        public void Start()
        {
            Connection = new YGOClient();
            _behavior = new GameBehavior(this);

            Connection.Connected += OnConnected;
            Connection.PacketReceived += OnPacketReceived;

            IPAddress target_address;
            try
            {
                target_address = IPAddress.Parse(_serverHost);
            }
            catch (System.Exception)
            {
                IPHostEntry _hostEntry = Dns.GetHostEntry(_serverHost);
                target_address = _hostEntry.AddressList.FirstOrDefault(findIPv4 => findIPv4.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork);
            }

            Connection.Connect(target_address, _serverPort);
        }

        private void OnConnected()
        {
            BinaryWriter packet = GamePacketFactory.Create(CtosMessage.PlayerInfo);
            packet.WriteUnicode(Username, 20);
            ///zdiy///
			packet.Write(12); // 写入额外参数，用于区分AI
			///zdiy///
			Connection.Send(packet);

            byte[] junk = { 0xCC, 0xCC, 0x00, 0x00, 0x00, 0x00 };
			packet = GamePacketFactory.Create(CtosMessage.JoinGame);
			packet.Write(_proVersion);
            packet.Write(junk);
            packet.WriteUnicode(_roomInfo, 30);
            Connection.Send(packet);
        }

        public void Tick()
        {
            Connection.Update();
        }

        public void Chat(string message)
        {
            byte[] content = Encoding.Unicode.GetBytes(message + "\0");
            BinaryWriter chat = GamePacketFactory.Create(CtosMessage.Chat);
            chat.Write(content);
            Connection.Send(chat);
        }

        public void Surrender()
        {
            Connection.Send(CtosMessage.Surrender);
        }

        private void OnPacketReceived(BinaryReader reader)
        {
            _behavior.OnPacket(reader);
        }
    }
}