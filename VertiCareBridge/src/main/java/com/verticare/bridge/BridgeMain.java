package com.verticare.bridge;

import com.alibaba.fastjson2.JSON;
import com.alibaba.fastjson2.JSONObject;
import org.apache.pulsar.client.api.Consumer;
import org.apache.pulsar.client.api.Message;
import org.apache.pulsar.client.api.PulsarClient;
import org.apache.pulsar.client.api.Schema;
import org.apache.pulsar.client.api.SubscriptionType;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;
import java.io.FileInputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collections;
import java.util.List;
import java.util.Properties;
import java.util.concurrent.TimeUnit;

public final class BridgeMain {
    private static final String DEFAULT_BROKER =
            "pulsar+ssl://iot-north-mq.heclouds.com:6651/";
    private static final PrintStream JSON_OUT;

    static {
        try {
            JSON_OUT = new PrintStream(System.out, true, "UTF-8");
        } catch (Exception exception) {
            throw new ExceptionInInitializerError(exception);
        }
    }

    private BridgeMain() {
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) {
            System.err.println("Usage: java -jar verticare-bridge.jar bridge.properties");
            System.exit(2);
        }

        Properties config = new Properties();
        try (FileInputStream input = new FileInputStream(args[0])) {
            config.load(input);
        }

        String accessId = required(config, "consumerGroupId");
        String secretKey = required(config, "consumerGroupKey");
        String subscriptionName = required(config, "subscriptionName");
        String productId = config.getProperty("productId", "").trim();
        String deviceName = config.getProperty("deviceName", "").trim();
        String broker = config.getProperty("brokerUrl", DEFAULT_BROKER).trim();
        String topic = config.getProperty("topic", accessId + "/iot/event").trim();

        long reconnectDelayMs = Long.parseLong(
                config.getProperty("reconnectDelayMs", "5000").trim());

        while (true) {
            try {
                runConsumer(broker, accessId, secretKey, subscriptionName,
                        topic, productId, deviceName);
            } catch (Exception exception) {
                System.err.println("Bridge connection failed: "
                        + exception.getMessage() + "; retrying in "
                        + reconnectDelayMs + " ms");
                Thread.sleep(Math.max(1000, reconnectDelayMs));
            }
        }
    }

    private static void runConsumer(String broker, String accessId,
                                    String secretKey, String subscriptionName,
                                    String topic, String productId,
                                    String deviceName) throws Exception {
        try (PulsarClient client = PulsarClient.builder()
                .serviceUrl(broker)
                .allowTlsInsecureConnection(true)
                .authentication(new OneNetAuthentication(accessId, secretKey))
                .build();
             Consumer<String> consumer = client.newConsumer(Schema.STRING)
                .topic(topic)
                .subscriptionName(subscriptionName)
                .subscriptionType(SubscriptionType.Failover)
                .autoUpdatePartitions(false)
                .subscribe()) {
            System.err.println("VertiCare bridge connected: " + topic);
            while (true) {
                Message<String> message = consumer.receive(1, TimeUnit.SECONDS);
                if (message == null)
                    continue;
                processMessage(consumer, message, secretKey,
                        productId, deviceName);
            }
        }
    }

    private static void processMessage(Consumer<String> consumer,
                                       Message<String> message,
                                       String secretKey, String productId,
                                       String deviceName) throws Exception {
        try {
            JSONObject envelope = JSON.parseObject(message.getValue());
            if (!verifySignature(envelope, secretKey)) {
                System.err.println("Dropped message with invalid signature.");
                consumer.acknowledge(message);
                return;
            }

            String decrypted = decrypt(envelope.getString("data"),
                    secretKey.substring(8, 24));
            JSONObject event = JSON.parseObject(decrypted);
            if (!"thingProperty".equals(event.getString("msgType"))) {
                consumer.acknowledge(message);
                return;
            }

            JSONObject subData = event.getJSONObject("subData");
            if (subData == null
                    || (!productId.isEmpty()
                    && !productId.equals(subData.getString("productId")))
                    || (!deviceName.isEmpty()
                    && !deviceName.equals(subData.getString("deviceName")))) {
                consumer.acknowledge(message);
                return;
            }

            JSONObject output = new JSONObject();
            output.put("type", "telemetry");
            output.put("productId", subData.getString("productId"));
            output.put("deviceName", subData.getString("deviceName"));
            output.put("receivedAt", System.currentTimeMillis());
            output.put("params", subData.getJSONObject("params"));
            JSON_OUT.println(output.toJSONString());
            JSON_OUT.flush();
            consumer.acknowledge(message);
        } catch (Exception exception) {
            System.err.println("Dropped malformed message: "
                    + exception.getMessage());
            consumer.acknowledge(message);
        }
    }

    private static String required(Properties config, String key) {
        String value = config.getProperty(key, "").trim();
        if (value.isEmpty())
            throw new IllegalArgumentException("Missing config: " + key);
        return value;
    }

    private static boolean verifySignature(JSONObject envelope, String secretKey)
            throws Exception {
        String expected = envelope.getString("sign");
        if (expected == null || expected.isEmpty())
            return false;

        List<String> keys = new ArrayList<>(envelope.keySet());
        keys.remove("sign");
        Collections.sort(keys);
        StringBuilder source = new StringBuilder();
        for (String key : keys) {
            if (source.length() > 0)
                source.append("||");
            source.append(key).append('=').append(envelope.get(key));
        }
        source.append("||").append(secretKey);

        byte[] digest = MessageDigest.getInstance("MD5")
                .digest(source.toString().getBytes(StandardCharsets.UTF_8));
        StringBuilder actual = new StringBuilder();
        for (byte value : digest)
            actual.append(String.format("%02x", value & 0xff));
        return actual.toString().equalsIgnoreCase(expected);
    }

    private static String decrypt(String encrypted, String aesKey) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/ECB/PKCS5Padding");
        cipher.init(Cipher.DECRYPT_MODE,
                new SecretKeySpec(aesKey.getBytes(StandardCharsets.UTF_8), "AES"));
        byte[] plain = cipher.doFinal(Base64.getDecoder().decode(encrypted));
        return new String(plain, StandardCharsets.UTF_8);
    }
}
