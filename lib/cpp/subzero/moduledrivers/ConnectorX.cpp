#include "subzero/moduledrivers/ConnectorX.h"

#include <hal/I2C.h>

using namespace ConnectorX;

ConnectorX::ConnectorXBoard::ConnectorXBoard(uint8_t slaveAddress,
                                             frc::I2C::Port port,
                                             units::second_t connectorXDelay)
    : _i2c(std::make_unique<frc::I2C>(port, slaveAddress)),
      _slaveAddress(slaveAddress), _delay{connectorXDelay},
      m_simDevice("Connector-X", static_cast<int>(port), slaveAddress) {
  // TODO: read config from device to get # of LEDs per port
  m_device.ports = {
      {
          .on = false,
          .currentZoneIndex = 0,
          .zones = {CachedZone({.offset = 0, .count = 43})},
      },
      {
          .on = false,
          .currentZoneIndex = 0,
          .zones = {CachedZone({.offset = 0, .count = 257})},
      },
  };

  if (m_simDevice) {
    m_simOn = m_simDevice.CreateBoolean("On", false, false);
    m_simColorR = m_simDevice.CreateInt("Red", false, -1);
    m_simColorG = m_simDevice.CreateInt("Green", false, -1);
    m_simColorB = m_simDevice.CreateInt("Blue", false, -1);
  }
}

bool ConnectorX::ConnectorXBoard::initialize() { return !_i2c->AddressOnly(); }

void ConnectorX::ConnectorXBoard::configureDigitalPin(DigitalPort port,
                                                      PinMode mode) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::DigitalSetup;
  cmd.commandData.commandDigitalSetup = {.port = static_cast<uint8_t>(port),
                                         .mode = static_cast<uint8_t>(mode)};

  sendCommand(cmd);
}

void ConnectorX::ConnectorXBoard::writeDigitalPin(DigitalPort port,
                                                  bool value) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::DigitalWrite;
  cmd.commandData.commandDigitalWrite = {.port = static_cast<uint8_t>(port),
                                         .value = static_cast<uint8_t>(value)};

  sendCommand(cmd);
}

bool ConnectorX::ConnectorXBoard::readDigitalPin(DigitalPort port) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::DigitalRead;
  cmd.commandData.commandDigitalRead = {.port = static_cast<uint8_t>(port)};

  Commands::Response res = sendCommand(cmd, true);
  return res.responseData.responseDigitalRead.value;
}

uint16_t ConnectorX::ConnectorXBoard::readAnalogPin(AnalogPort port) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::ReadAnalog;
  cmd.commandData.commandReadAnalog = {.port = static_cast<uint8_t>(port)};

  Commands::Response res = sendCommand(cmd, true);
  return res.responseData.responseReadAnalog.value;
}

CachedZone &ConnectorX::ConnectorXBoard::setCurrentZone(LedPort port,
                                                        uint8_t zoneIndex,
                                                        bool reversed,
                                                        bool setReversed) {
  setLedPort(port);
  delaySeconds(_delay);
  auto &currentPort = getCurrentCachedPort();
  auto &currentZone = getCurrentZone();

  if (zoneIndex > currentPort.zones.size()) {
    return currentZone;
  }

  ConsoleWriter.logVerbose("ConnectorX", "Setting new zone index to %u",
                           zoneIndex);
  currentPort.currentZoneIndex = zoneIndex;
  currentZone = getCurrentZone();

  ConsoleWriter.logVerbose("ConnectorX", "Got zone %s",
                           currentZone.toString().c_str());

  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::SetPatternZone;
  cmd.commandData.commandSetPatternZone.zoneIndex = zoneIndex;
  cmd.commandData.commandSetPatternZone.reversed = currentZone.reversed ? 1 : 0;
  if (setReversed) {
    currentZone.reversed = reversed;
    cmd.commandData.commandSetPatternZone.reversed = reversed ? 1 : 0;
  }

  sendCommand(cmd);

  return getCurrentZone();
}

void ConnectorX::ConnectorXBoard::syncZones(LedPort port,
                                            const std::vector<uint8_t> &zones) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::SyncStates;
  cmd.commandData.commandSyncZoneStates.zoneCount = zones.size();

  for (uint8_t i = 0; i < zones.size(); i++) {
    cmd.commandData.commandSyncZoneStates.zones[i] = zones[i];
  }

  sendCommand(cmd);
}

void ConnectorX::ConnectorXBoard::createZones(
    LedPort port, std::vector<ConnectorX::Commands::NewZone> &&newZones) {
  setLedPort(port);
  delaySeconds(_delay);

  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::SetNewZones;
  cmd.commandData.commandSetNewZones.zoneCount = newZones.size();

  for (uint8_t i = 0; i < newZones.size(); i++) {
    cmd.commandData.commandSetNewZones.zones[i] = newZones[i];
  }

  auto &currentPort = getCurrentCachedPort();

  std::vector<CachedZone> zones;
  zones.reserve(newZones.size());

  for (auto &zone : newZones) {
    zones.push_back(CachedZone(zone));
  }

  currentPort.zones = zones;

  ConsoleWriter.logVerbose("ConnectorX", "Created zones: %s", "");
  for (auto &zone : currentPort.zones) {
    ConsoleWriter.logVerbose("ConnectorX", "Zone: %s", zone.toString().c_str());
  }
}

void ConnectorX::ConnectorXBoard::setLedPort(LedPort port) {
  if (static_cast<uint8_t>(port) != m_device.currentPort) {
    ConsoleWriter.logVerbose("ConnectorX", "Setting new LED port to %u",
                             static_cast<uint8_t>(port));
    m_device.currentPort = static_cast<uint8_t>(port);
    Commands::Command cmd;
    cmd.commandType = Commands::CommandType::SetLedPort;
    cmd.commandData.commandSetLedPort.port = static_cast<uint8_t>(port);
    sendCommand(cmd);
  }
}

void ConnectorX::ConnectorXBoard::setOn() {
  if (m_simDevice) {
    m_simOn.Set(true);
  }

  bool shouldSet = false;

  for (auto &port : m_device.ports) {
    if (!port.on) {
      shouldSet = true;
    }

    port.on = true;
  }

  if (shouldSet) {
    ConsoleWriter.logVerbose("ConnectorX", "Setting to on %s", "");
    Commands::Command cmd;
    cmd.commandType = Commands::CommandType::On;
    cmd.commandData.commandOn = {};

    // TODO: move into a loop
    setLedPort(LedPort::P0);
    delaySeconds(_delay);
    sendCommand(cmd);

    setLedPort(LedPort::P1);
    delaySeconds(_delay);
    sendCommand(cmd);
  }
}

void ConnectorX::ConnectorXBoard::setOff() {
  if (m_simDevice) {
    m_simOn.Set(false);
  }

  bool shouldSet = false;

  for (auto &port : m_device.ports) {
    if (port.on) {
      shouldSet = true;
    }

    port.on = false;
  }

  if (shouldSet) {
    ConsoleWriter.logVerbose("ConnectorX", "Setting to off %s", "");
    Commands::Command cmd;
    cmd.commandType = Commands::CommandType::Off;
    cmd.commandData.commandOff = {};
    sendCommand(cmd);
  }
}

void ConnectorX::ConnectorXBoard::setPattern(LedPort port, PatternType pattern,
                                             bool oneShot, int16_t delay,
                                             uint8_t zoneIndex, bool reversed) {
  auto &zone = setCurrentZone(port, zoneIndex, reversed, true);

  delaySeconds(_delay);

  if (zone.pattern != pattern) {
    ConsoleWriter.logVerbose("ConnectorX", "Setting new pattern to %u",
                             static_cast<uint8_t>(pattern));
    zone.pattern = pattern;
    Commands::Command cmd;
    cmd.commandType = Commands::CommandType::Pattern;
    cmd.commandData.commandPattern.pattern = static_cast<uint8_t>(pattern);
    cmd.commandData.commandPattern.oneShot = static_cast<uint8_t>(oneShot);
    cmd.commandData.commandPattern.delay = delay;
    sendCommand(cmd);
  }
}

void ConnectorX::ConnectorXBoard::setColor(LedPort port, uint8_t red,
                                           uint8_t green, uint8_t blue,
                                           uint8_t zoneIndex) {
  if (m_simDevice) {
    m_simColorR.Set(red);
    m_simColorG.Set(green);
    m_simColorB.Set(blue);
  }

  auto &zone = getCurrentZone();
  auto newColor = frc::Color8Bit(red, green, blue);

  setLedPort(port);
  delaySeconds(_delay);

  ConsoleWriter.logVerbose("ConnectorX", "Zone %u cur color=%s | new color=%s",
                           zoneIndex, zone.color.HexString().c_str(),
                           newColor.HexString().c_str());

  if (zone.color != newColor) {
    ConsoleWriter.logVerbose("ConnectorX", "Setting new color to %s",
                             newColor.HexString().c_str());
    zone.color = newColor;
    Commands::Command cmd;
    cmd.commandType = Commands::CommandType::ChangeColor;
    cmd.commandData.commandColor.red = red;
    cmd.commandData.commandColor.green = green;
    cmd.commandData.commandColor.blue = blue;

    sendCommand(cmd);
  }
}

bool ConnectorX::ConnectorXBoard::getPatternDone(LedPort port) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::ReadPatternDone;
  cmd.commandData.commandReadPatternDone = {};

  Commands::Response res = sendCommand(cmd, true);
  return res.responseData.responsePatternDone.done;
}

void ConnectorX::ConnectorXBoard::setConfig(Commands::Configuration config) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::SetConfig;
  cmd.commandData.commandSetConfig.config = config;

  sendCommand(cmd);
}

Commands::Configuration ConnectorX::ConnectorXBoard::readConfig() {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::ReadConfig;
  cmd.commandData.commandReadConfig = {};

  Commands::Response res = sendCommand(cmd, true);
  return res.responseData.responseReadConfiguration.config;
}

void ConnectorX::ConnectorXBoard::sendRadioMessage(Message message) {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::RadioSend;
  cmd.commandData.commandRadioSend.msg = message;

  sendCommand(cmd);
}

Message ConnectorX::ConnectorXBoard::getLatestRadioMessage() {
  Commands::Command cmd;
  cmd.commandType = Commands::CommandType::RadioGetLatestReceived;
  cmd.commandData.commandRadioGetLatestReceived = {};

  Commands::Response res = sendCommand(cmd, true);
  return res.responseData.responseRadioLastReceived.msg;
}

Commands::Response
ConnectorX::ConnectorXBoard::sendCommand(Commands::Command command,
                                         bool expectResponse) {
  using namespace Commands;

  _lastCommand = command.commandType;
  uint8_t sendLen = 0;
  uint8_t recSize = 0;
  Response response;
  response.commandType = command.commandType;

  uint8_t sendBuf[sizeof(CommandData) + 1];
  sendBuf[0] = static_cast<uint8_t>(command.commandType);

  switch (command.commandType) {
  case CommandType::On:
  case CommandType::Off:
    sendLen = 0;
    break;
  case CommandType::ReadConfig:
    sendLen = 0;
    recSize = sizeof(ResponseReadConfiguration);
    break;
  case CommandType::ReadPatternDone:
    sendLen = 0;
    recSize = sizeof(ResponsePatternDone);
    break;
  case CommandType::RadioGetLatestReceived:
    sendLen = 0;
    recSize = sizeof(ResponseRadioLastReceived);
    break;
  case CommandType::SetLedPort:
    sendLen = 1;
    break;
  case CommandType::ReadAnalog:
    sendLen = 1;
    recSize = sizeof(ResponseReadAnalog);
    break;
  case CommandType::DigitalRead:
    sendLen = 1;
    recSize = sizeof(ResponseDigitalRead);
    break;
  case CommandType::DigitalWrite:
    sendLen = sizeof(CommandDigitalWrite);
    break;
  case CommandType::DigitalSetup:
    sendLen = sizeof(CommandDigitalSetup);
    break;
  case CommandType::Pattern:
    sendLen = sizeof(CommandPattern);
    break;
  case CommandType::ChangeColor:
    sendLen = sizeof(CommandColor);
    break;
  case CommandType::SetConfig:
    sendLen = sizeof(CommandSetConfig);
    break;
  case CommandType::RadioSend:
    sendLen = sizeof(CommandRadioSend);
    break;
  case CommandType::GetColor:
    sendLen = 0;
    break;
  case CommandType::GetPort:
    sendLen = 0;
    break;
  case CommandType::SetPatternZone:
    sendLen = 3;
    break;
  case CommandType::SetNewZones:
    sendLen =
        sizeof(CommandSetNewZones::zoneCount) +
        command.commandData.commandSetNewZones.zoneCount * sizeof(NewZone);
    break;
  case CommandType::SyncStates:
    sendLen =
        sizeof(CommandSyncZoneStates::zoneCount) +
        command.commandData.commandSyncZoneStates.zoneCount * sizeof(uint8_t);
    break;
  }

  memcpy(sendBuf + 1, &command.commandData, sendLen);

  std::ostringstream stream;
  for (uint8_t i = 0; i < sendLen + 1; i++) {
    stream << std::to_string(sendBuf[i]) << " ";
  }
  std::string result = stream.str();
  ConsoleWriter.logVerbose("ConnectorX", "Sending data: %s with len=%d",
                           result.c_str(), sendLen + 1);
  if (recSize == 0) {
    ConsoleWriter.logVerbose("ConnectorX", "FPGA TIMESTAMP BEFORE %f",
                             frc::Timer::GetFPGATimestamp().value());
    int result =
        HAL_WriteI2C(HAL_I2C_kMXP, _slaveAddress, sendBuf, sendLen + 1);

    if (result == -1) {
      ConsoleWriter.logError("ConnectorX", "Write Result Failed %d errno=%s",
                             result, std::strerror(errno));
    }

    ConsoleWriter.logVerbose("ConnectorX", "FPGA TIMESTAMP AFTER %f",
                             frc::Timer::GetFPGATimestamp().value());
    return response;
  }

  ConsoleWriter.logVerbose("ConnectorX", "UNREACHABLE %s", "");

  _i2c->Transaction(sendBuf, sendLen + 1,
                    reinterpret_cast<uint8_t *>(&response.responseData),
                    recSize);
  return response;
}