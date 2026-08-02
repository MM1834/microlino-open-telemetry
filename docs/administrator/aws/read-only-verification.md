# AWS Read-only Verification Checklist

> **Status:** Prepared, not executed
>
> **Audience:** Authorized AWS administrator and maintainer

## Purpose

Confirm whether deployed AWS state matches the repository without creating,
updating, deleting or invoking application workflows. Execution requires separate
maintainer approval and an AWS identity with read-only permissions.

## Safety rules

- Confirm AWS account and Region before every query group.
- Do not display access tokens, Cognito user attributes, telemetry payloads,
  certificates, private keys or Lambda environment values in shared logs.
- Do not use CloudFormation deploy/update/delete, change sets, Cognito user
  creation, IoT publish, API requests or Lambda invocation in this checklist.
- Store any captured evidence outside public Git unless it is reviewed and redacted.
- Record command time, caller identity, account, Region, stack ID and Git commit.

## Planned evidence groups

### 1. Caller and scope

- identify the caller ARN and AWS account;
- confirm configured Region;
- identify candidate MOT CloudFormation stacks by tags/name;
- stop if account, Region or stack ownership is ambiguous.

### 2. CloudFormation

- read stack status, creation/update time and stack ID;
- read parameter keys and non-secret values;
- read outputs;
- list logical/physical resources and resource status;
- compare the deployed template digest/content with the repository template;
- inspect drift-detection status only if an existing result is available.

Starting a new drift-detection operation is not included because it creates an AWS
control-plane operation, even though it does not modify application resources.

### 3. Cognito

- describe User Pool security and deletion-protection settings;
- describe app client OAuth flows, token validity and redirect/logout URLs;
- describe the managed-login domain;
- confirm self-registration setting without listing users;
- do not export user records or attributes.

### 4. API Gateway

- read HTTP API CORS and protocol settings;
- list routes and authorizer attachment;
- read stage throttling and access-log configuration;
- read WebSocket route selection, routes, authorizer and stage;
- do not call health, vehicle or WebSocket endpoints during inventory.

### 5. Lambda and logging

- read function runtime, handler, role ARN, code update time and configuration;
- do not retrieve or display environment variable values unless individually
  reviewed as non-secret;
- read explicit log-group retention and encryption settings;
- inspect whether API Gateway access logs exist;
- do not tail or query log events during inventory.

### 6. DynamoDB

- describe table keys, indexes, encryption, TTL, billing, backups/PITR and deletion
  protection;
- do not scan or read table items.

### 7. AWS IoT

- describe the IoT Rule SQL, enabled state and Lambda target;
- read data endpoint metadata;
- count or list Thing/certificate identifiers only when required for inventory;
- do not retrieve certificate material, policies with unrelated resources or MQTT
  payloads;
- do not publish, subscribe, attach, detach, activate or deactivate anything.

### 8. IAM

- read the three declared Lambda role trust and permission policies;
- compare actual actions/resources with the template;
- identify additional attached policies or permission boundaries;
- do not simulate, attach or modify policies.

## Expected output

Produce a redacted verification record with one result per item:

- Confirmed deployed
- Deployed with different parameter/configuration
- Declared but not deployed
- Additional deployed resource
- Not checked
- Access denied

The record must keep facts separate from recommendations.

## Approval gate

This checklist is documentation only. No AWS command has been executed during
DOC-001. Execution begins only after explicit approval of account, Region, role and
evidence-handling location.
