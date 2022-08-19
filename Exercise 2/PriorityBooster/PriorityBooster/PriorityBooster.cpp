#include <ntifs.h>
#include <ntddk.h>
#include "PriorityBoosterCommon.h"

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS CreateDeviceObject(_In_ UNICODE_STRING deviceName,
	_Out_ PDEVICE_OBJECT deviceObject, _In_ PDRIVER_OBJECT driverObject);

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driverObject) {
	/*
	Set an unload routine
	Set dispatch routines (Which operations the driver supports: Create, Read, Write)
	Create a device object
	Create a symbolic link to the device object
	*/

	// Set unload and dispatch routines
	driverObject->DriverUnload = PriorityBoosterUnload;
	driverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	driverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");
	PDEVICE_OBJECT deviceObject{};
	auto status = CreateDeviceObject(devName, _Out_ deviceObject, driverObject);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to create device object (0x%08X\n", status));
		return status;
	}

	// Create symbolic link to make device object accessible to user mode callers
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	status = IoCreateSymbolicLink(&symLink, &devName);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to create symbolic link (0x%08X", status));
		IoDeleteDevice(deviceObject);
	}

	KdPrint(("Driver loaded successfully\n"));
	return STATUS_SUCCESS;
}

NTSTATUS CreateDeviceObject(
	_In_ UNICODE_STRING deviceName,
	_Out_ PDEVICE_OBJECT deviceObject,
	_In_ PDRIVER_OBJECT driverObject) {

	const ULONG extraBytes = 0;
	const ULONG driverCharacteristics = 0; // Software drivers specify 0 almost every time

	return IoCreateDevice(
		driverObject,
		extraBytes, // No need for extra bytes
		&deviceName,
		FILE_DEVICE_UNKNOWN, // default device type for software drivers
		driverCharacteristics,
		FALSE,
		&deviceObject
	);
}

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject) {
	// delete in reverse order
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
	KdPrint(("Driver unloaded successfully\n"));
}

void completeIoRequest(_In_ PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	completeIoRequest(Irp);
	return STATUS_SUCCESS;
}

BOOLEAN hasValidThreadDataSize(_In_ PIO_STACK_LOCATION stack) {
	auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
	return (len == sizeof(ThreadData));
}

BOOLEAN isInThreadDataRange(_In_ ThreadData* data) {
	return (data != nullptr && data->Priority > 1 && data->Priority < 31);
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY: {

			if (!hasValidThreadDataSize(stack)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			auto data = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
			if (!isInThreadDataRange(data)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			PETHREAD thread;
			status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &thread);
			if (!NT_SUCCESS(status))
				break;

			// increases object reference count
			KeSetPriorityThread((PKTHREAD)thread, data->Priority);
			// decreases object reference count, so thread doesn't leak
			ObDereferenceObject(thread);

			KdPrint(("Thread Priority change for %d to %d succeeded!\n", data->ThreadId, data->Priority));
			break;
		}
		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}

	completeIoRequest(Irp);
	return status;
}