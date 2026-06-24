# VertiCare Bridge

OneNet服务端订阅使用定制Apache Pulsar协议，而不是普通MQTT。Bridge负责：

- 使用消费组ID和Key完成Pulsar鉴权
- 校验OneNet消息MD5签名
- 使用AES-ECB解密消息
- 筛选 `thingProperty`
- 按产品ID和设备名过滤数据
- 向Qt标准输出发送一行一条的JSON

## 配置

复制 `bridge.properties.example` 为 `bridge.properties`，填写消费组信息。

## 构建

```powershell
mvn -DskipTests package
```

输出文件：

```text
target/verticare-bridge.jar
```

## 运行

```powershell
java -jar target/verticare-bridge.jar bridge.properties
```

`bridge.properties`包含消费组Key，已被Git忽略。
