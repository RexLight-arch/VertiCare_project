package com.verticare.bridge;

import org.apache.pulsar.client.api.Authentication;
import org.apache.pulsar.client.api.AuthenticationDataProvider;
import org.apache.pulsar.client.api.EncodedAuthenticationParameterSupport;
import org.apache.pulsar.client.api.PulsarClientException;
import org.apache.pulsar.shade.org.apache.commons.codec.digest.DigestUtils;

import java.io.IOException;
import java.util.Map;

final class OneNetAuthentication implements Authentication, EncodedAuthenticationParameterSupport {
    private static final long serialVersionUID = 1L;
    private final String accessId;
    private final String secretKey;

    OneNetAuthentication(String accessId, String secretKey) {
        this.accessId = accessId;
        this.secretKey = secretKey;
    }

    @Override
    public String getAuthMethodName() {
        return "iot-auth";
    }

    @Override
    public AuthenticationDataProvider getAuthData() throws PulsarClientException {
        final String password = DigestUtils.sha256Hex(
                accessId + DigestUtils.sha256Hex(secretKey)).substring(4, 20);
        final String token = String.format(
                "{\"tenant\":\"%s\",\"password\":\"%s\"}", accessId, password);

        return new AuthenticationDataProvider() {
            private static final long serialVersionUID = 1L;

            @Override
            public boolean hasDataFromCommand() {
                return true;
            }

            @Override
            public String getCommandData() {
                return token;
            }
        };
    }

    @Override
    public void configure(String encodedAuthParamString) {
    }

    @Deprecated
    @Override
    public void configure(Map<String, String> authParams) {
    }

    @Override
    public void start() throws PulsarClientException {
    }

    @Override
    public void close() throws IOException {
    }
}
