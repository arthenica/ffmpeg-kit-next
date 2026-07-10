import type {TurboModule} from 'react-native';
import {TurboModuleRegistry} from 'react-native';

// Structured data (sessions, logs, statistics, media information) is passed as
// untyped `Object` by design; the JS wrapper classes in index.js shape it.
// Binary data (protocols) is exchanged as base64 `string`.
export interface Spec extends TurboModule {
  // Events (NativeEventEmitter plumbing)
  addListener(eventName: string): void;
  removeListeners(count: number): void;

  // AbstractSession
  abstractSessionGetEndTime(sessionId: number): Promise<number>;
  abstractSessionGetDuration(sessionId: number): Promise<number>;
  abstractSessionGetAllLogs(sessionId: number, waitTimeout?: number): Promise<Object[]>;
  abstractSessionGetLogs(sessionId: number): Promise<Object[]>;
  abstractSessionGetAllLogsAsString(sessionId: number, waitTimeout?: number): Promise<string>;
  abstractSessionGetState(sessionId: number): Promise<number>;
  abstractSessionGetReturnCode(sessionId: number): Promise<number>;
  abstractSessionGetFailStackTrace(sessionId: number): Promise<string>;
  thereAreAsynchronousMessagesInTransmit(sessionId: number): Promise<boolean>;

  // ArchDetect
  getArch(): Promise<string>;

  // Session creation / statistics
  ffmpegSession(commandArguments: string[]): Promise<Object>;
  ffmpegSessionGetAllStatistics(sessionId: number, waitTimeout?: number): Promise<Object[]>;
  ffmpegSessionGetStatistics(sessionId: number): Promise<Object[]>;
  ffprobeSession(commandArguments: string[]): Promise<Object>;
  mediaInformationSession(commandArguments: string[]): Promise<Object>;
  mediaInformationJsonParserFrom(ffprobeJsonOutput: string): Promise<Object>;
  mediaInformationJsonParserFromWithError(ffprobeJsonOutput: string): Promise<Object>;

  // Redirection / logs / statistics toggles
  enableRedirection(): Promise<void>;
  disableRedirection(): Promise<void>;
  enableLogs(): Promise<void>;
  disableLogs(): Promise<void>;
  enableStatistics(): Promise<void>;
  disableStatistics(): Promise<void>;

  // Fonts
  setFontconfigConfigurationPath(path: string): Promise<void>;
  setFontDirectory(fontDirectoryPath: string, fontNameMap: Object): Promise<void>;
  setFontDirectoryList(fontDirectoryList: string[], fontNameMap: Object): Promise<void>;

  // Pipes
  registerNewFFmpegPipe(): Promise<string>;
  closeFFmpegPipe(ffmpegPipePath: string): Promise<void>;

  // Version / build / env
  getFFmpegVersion(): Promise<string>;
  isLTSBuild(): Promise<boolean>;
  getBuildDate(): Promise<string>;
  setEnvironmentVariable(variableName: string, variableValue: string): Promise<void>;
  ignoreSignal(signalValue: number): Promise<void>;

  // Execute
  ffmpegSessionExecute(sessionId: number): Promise<void>;
  ffprobeSessionExecute(sessionId: number): Promise<void>;
  mediaInformationSessionExecute(sessionId: number, waitTimeout?: number): Promise<void>;
  asyncFFmpegSessionExecute(sessionId: number): Promise<void>;
  asyncFFprobeSessionExecute(sessionId: number): Promise<void>;
  asyncMediaInformationSessionExecute(sessionId: number, waitTimeout?: number): Promise<void>;

  // Log level / history size
  getLogLevel(): Promise<number>;
  setLogLevel(level: number): Promise<void>;
  getSessionHistorySize(): Promise<number>;
  setSessionHistorySize(sessionHistorySize: number): Promise<void>;

  // Session registry
  getSession(sessionId: number): Promise<Object>;
  getLastSession(): Promise<Object>;
  getLastCompletedSession(): Promise<Object>;
  getSessions(): Promise<Object[]>;
  clearSessions(): Promise<void>;
  getSessionsByState(sessionState: number): Promise<Object[]>;
  getLogRedirectionStrategy(): Promise<number>;
  setLogRedirectionStrategy(logRedirectionStrategy: number): Promise<void>;
  messagesInTransmit(sessionId: number): Promise<number>;
  getPlatform(): Promise<string>;
  writeToPipe(inputPath: string, namedPipePath: string): Promise<number>;

  // Storage Access Framework (Android)
  selectDocument(writable: boolean, title?: string, type?: string, extraTypes?: string[]): Promise<string>;
  getSafParameter(uriString: string, openMode: string, reusable?: boolean): Promise<string>;
  unregisterSafProtocolUrl(safUrl: string): Promise<void>;

  // Camera (Android)
  getSupportedCameraIds(): Promise<string[]>;

  // Cancel
  cancel(): Promise<void>;
  cancelSession(sessionId: number): Promise<void>;

  // Session lists by type / media information
  getFFmpegSessions(): Promise<Object[]>;
  getFFprobeSessions(): Promise<Object[]>;
  getMediaInformationSessions(): Promise<Object[]>;
  getMediaInformation(sessionId: number): Promise<Object>;

  // Packages
  getPackageName(): Promise<string>;
  getExternalLibraries(): Promise<string[]>;

  // FFmpegKitInputBuffer
  inputBufferFromByteArray(data: string, extension?: string): Promise<string>;
  inputBufferClose(url: string): Promise<void>;

  // FFmpegKitOutputBuffer
  outputBufferCreate(extension?: string, initialCapacity?: number, maxCapacity?: number): Promise<string>;
  outputBufferGetSize(url: string): Promise<number>;
  outputBufferToByteArray(url: string): Promise<string>;
  outputBufferClose(url: string): Promise<void>;

  // FFmpegKitStreamInput
  streamInputCreate(extension?: string, capacity?: number): Promise<string>;
  streamInputWrite(url: string, data: string, timeoutMs?: number): Promise<number>;
  streamInputCloseInput(url: string): Promise<void>;
  streamInputClose(url: string): Promise<void>;

  // FFmpegKitStreamOutput
  streamOutputCreate(extension?: string, capacity?: number): Promise<string>;
  streamOutputRead(url: string, maxBytes: number, timeoutMs?: number): Promise<string>;
  streamOutputClose(url: string): Promise<void>;

  // Lifecycle
  uninit(): Promise<void>;
}

export default TurboModuleRegistry.getEnforcing<Spec>('FFmpegKitReactNativeModule');
