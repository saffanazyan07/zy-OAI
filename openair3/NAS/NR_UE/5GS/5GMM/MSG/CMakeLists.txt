add_library(fgs_5gmm_lib OBJECT
    fgs_service_request.c
    RegistrationRequest.c
    RegistrationAccept.c
    FGSIdentityResponse.c
    FGSAuthenticationResponse.c
    FGSNASSecurityModeComplete.c
    RegistrationComplete.c
    FGSUplinkNasTransport.c
    FGSDeregistrationRequestUEOriginating.c
)

target_link_libraries(fgs_5gmm_lib PUBLIC fgs_5gmm_ies_lib)

target_include_directories(fgs_5gmm_lib PRIVATE ../IES/ ../../)
