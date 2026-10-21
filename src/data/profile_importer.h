#ifndef KSV_PROFILE_IMPORTER_H
#define KSV_PROFILE_IMPORTER_H
#include "interfaces/i_profile_importer.h"
#include "interfaces/i_run_ingestor.h"
namespace ksv::data {
    class ProfileImporter final : public IProfileImporter {
    public:
        ProfileImporter(std::shared_ptr<application::IFileService>, std::shared_ptr<IRunIngestor>);
        [[nodiscard]] application::ProfileImportResult run(const application::ProfileImportRequest &,
            const application::ProfileImportProgress &) const override;
    private:
        std::shared_ptr<application::IFileService> m_files;
        std::shared_ptr<IRunIngestor> m_ingestor;
    };
}
#endif
