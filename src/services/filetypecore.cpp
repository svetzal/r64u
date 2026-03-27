/**
 * @file filetypecore.cpp
 * @brief Implementation of pure C64 file type detection and capability functions.
 */

#include "filetypecore.h"

#include <QCoreApplication>

namespace filetype {

FileType detectFromFilename(const QString &filename)
{
    QString ext = filename.section('.', -1).toLower();

    if (ext == "sid" || ext == "psid" || ext == "rsid") {
        return FileType::SidMusic;
    }
    if (ext == "mod" || ext == "xm" || ext == "s3m" || ext == "it") {
        return FileType::ModMusic;
    }
    if (ext == "prg" || ext == "p00") {
        return FileType::Program;
    }
    if (ext == "crt") {
        return FileType::Cartridge;
    }
    if (ext == "d64" || ext == "d71" || ext == "d81" || ext == "g64" || ext == "g71") {
        return FileType::DiskImage;
    }
    if (ext == "tap" || ext == "t64") {
        return FileType::TapeImage;
    }
    if (ext == "rom" || ext == "bin") {
        return FileType::Rom;
    }
    if (ext == "cfg") {
        return FileType::Config;
    }

    return FileType::Unknown;
}

QString displayName(FileType type)
{
    switch (type) {
    case FileType::Directory:
        return QCoreApplication::translate("filetype", "Folder");
    case FileType::SidMusic:
        return QCoreApplication::translate("filetype", "SID Music");
    case FileType::ModMusic:
        return QCoreApplication::translate("filetype", "MOD Music");
    case FileType::Program:
        return QCoreApplication::translate("filetype", "Program");
    case FileType::Cartridge:
        return QCoreApplication::translate("filetype", "Cartridge");
    case FileType::DiskImage:
        return QCoreApplication::translate("filetype", "Disk Image");
    case FileType::TapeImage:
        return QCoreApplication::translate("filetype", "Tape Image");
    case FileType::Rom:
        return QCoreApplication::translate("filetype", "ROM");
    case FileType::Config:
        return QCoreApplication::translate("filetype", "Configuration");
    default:
        return QCoreApplication::translate("filetype", "File");
    }
}

Capabilities capabilities(FileType type)
{
    switch (type) {
    case FileType::SidMusic:
    case FileType::ModMusic: {
        Capabilities caps;
        caps.canPlay = true;
        return caps;
    }
    case FileType::Program:
    case FileType::Cartridge: {
        Capabilities caps;
        caps.canRun = true;
        return caps;
    }
    case FileType::DiskImage: {
        Capabilities caps;
        caps.canRun = true;
        caps.canMount = true;
        return caps;
    }
    case FileType::Config: {
        Capabilities caps;
        caps.canLoadConfig = true;
        return caps;
    }
    default:
        return {};
    }
}

DefaultAction defaultAction(FileType type)
{
    switch (type) {
    case FileType::SidMusic:
    case FileType::ModMusic:
        return DefaultAction::Play;
    case FileType::Program:
    case FileType::Cartridge:
        return DefaultAction::Run;
    case FileType::DiskImage:
        return DefaultAction::Mount;
    case FileType::Config:
        return DefaultAction::LoadConfig;
    default:
        return DefaultAction::None;
    }
}

}  // namespace filetype
