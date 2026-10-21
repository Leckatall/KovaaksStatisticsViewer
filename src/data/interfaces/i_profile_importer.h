#ifndef KSV_I_PROFILE_IMPORTER_H
#define KSV_I_PROFILE_IMPORTER_H
#include "app/contracts/profile_import.h"
namespace ksv::data {
    class IProfileImporter {
    public:
        virtual ~IProfileImporter() = default;
        [[nodiscard]] virtual application::ProfileImportResult run(
            const application::ProfileImportRequest &, const application::ProfileImportProgress &) const = 0;
    };
}
#endif
