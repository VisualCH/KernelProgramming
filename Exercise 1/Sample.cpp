#include <ntddk.h>

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);
	KdPrint(("Sample driver Unload called\n"));
}

void PrintVersion() {
	RTL_OSVERSIONINFOW osVersionInfo{};
	if (RtlGetVersion(&osVersionInfo) != STATUS_SUCCESS) {
		KdPrint(("RtlGetVersion failed.\n"));
		return;
	}
	KdPrint(("Windows OS version: \nMajor: %lu\nMinor: %lu\nBuild Number: %lu\n", 
		osVersionInfo.dwMajorVersion,
		osVersionInfo.dwMinorVersion,
		osVersionInfo.dwBuildNumber));
}

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = SampleUnload;
	KdPrint(("Sample driver initialized successfully\n"));
	PrintVersion();

	return STATUS_SUCCESS;
}