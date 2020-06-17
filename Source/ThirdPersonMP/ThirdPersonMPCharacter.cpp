// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ThirdPersonMPCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

//These provide required functionality for variable replication as well as access to the AddOnscreenDebugMessage function in GEngine, which we will use to output messages to the screen.
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

//This will enable our Character class to recognize the projectile's type and spawn it.
#include "ThirdPersonMPProjectile.h"


//////////////////////////////////////////////////////////////////////////
// AThirdPersonMPCharacter

AThirdPersonMPCharacter::AThirdPersonMPCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	//Initialize the player's Health. Any time a new copy of this Character is created, its current health will be set to its maximum health value.
	MaxHealth = 100.0f;
	CurrentHealth = MaxHealth;

	//These will initialize the variables necessary to handle firing the projectile.
	//Initialize projectile class
	ProjectileClass = AThirdPersonMPProjectile::StaticClass();
	//Initialize fire rate
	FireRate = 0.25f;
	bIsFiringWeapon = false;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AThirdPersonMPCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AThirdPersonMPCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AThirdPersonMPCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AThirdPersonMPCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AThirdPersonMPCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AThirdPersonMPCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AThirdPersonMPCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AThirdPersonMPCharacter::OnResetVR);

	// Handle firing projectiles
	//This binds StartFire to the Fire Input Action we created in the first step of this section, enabling the user to activate it.
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AThirdPersonMPCharacter::StartFire);
}


void AThirdPersonMPCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AThirdPersonMPCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AThirdPersonMPCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AThirdPersonMPCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AThirdPersonMPCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AThirdPersonMPCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AThirdPersonMPCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

//////////////////////////////////////////////////////////////////////////
// Replicated Properties


//The GetLifetimeReplicatedProps function is responsible for replicating any properties we designate with the Replicated specifier, and enables us to configure how a property will replicate. Here we are using the most basic implementation for CurrentHealth. If at any time you add more properties that need to be replicated, you must add them to this function as well.

//You must call the Super version of GetLifetimeReplicatedProps, or inherited properties from your Actor's parent class will not replicate, even if the parent class designates them as being replicated.
void AThirdPersonMPCharacter::GetLifetimeReplicatedProps(TArray <FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AThirdPersonMPCharacter, CurrentHealth);
}


//StartFire is the function that players call on their local machine in order to initiate the firing process, and it restricts how often the user is allowed to call HandleFire based on the following criteria :
//The user cannot fire a projectile if they are already in the middle of firing.This is designated with bFiringWeapon, which is set to true when StartFire is called.
//bFiringWeapon is only set to false when StopFire is called.
//StopFire is called when a timer with a length of FireRate finishes.
//This means that when the user fires a projectile, they must wait a number of seconds equal to FireRate before they can fire again.This will function consistently regarldess of what kind of input StartFire is bound to.For example, if the user binds the "Fire" command to a scroll wheel or similarly inappropriate input, or if they mash the button repeatedly, this function will still execute at an acceptable interval of time and not overflow the user's queue for reliable functions with calls to HandleFire.
//Because HandleFire is a Server RPC, its implementation in the CPP file must have the suffix _Implementation added to the function name.Our implementation here uses the Character's Control Rotation to get the direction that the camera is facing, then spawn the projectile facing in that direction, enabling the player to aim. The projectile's Projectile Movement Component then handles moving it in that direction.

void AThirdPersonMPCharacter::StartFire()
{
	if (!bIsFiringWeapon)
	{
		bIsFiringWeapon = true;
		UWorld* World = GetWorld();
		World->GetTimerManager().SetTimer(FiringTimer, this, &AThirdPersonMPCharacter::StopFire, FireRate, false);
		HandleFire();
	}
}

void AThirdPersonMPCharacter::StopFire()
{
	bIsFiringWeapon = false;
}

void AThirdPersonMPCharacter::HandleFire_Implementation()
{
	FVector spawnLocation = GetActorLocation() + (GetControlRotation().Vector() * 100.0f) + (GetActorUpVector() * 50.0f);
	FRotator spawnRotation = GetControlRotation();

	FActorSpawnParameters spawnParameters;
	spawnParameters.Instigator = Instigator;
	spawnParameters.Owner = this;

	AThirdPersonMPProjectile* spawnedProjectile = GetWorld()->SpawnActor<AThirdPersonMPProjectile>(spawnLocation, spawnRotation, spawnParameters);
}

//We will be using this function to perform updates in response to changes to the player's CurrentHealth. Currently its functionality is limited to onscreen debug messages, but additional functionality could be added, like an OnDeath function that is called on all machines in order to trigger a death animation. Note that OnHealthUpdate is not replicated, and we will need to manually call it on all devices.
void AThirdPersonMPCharacter::OnHealthUpdate()
{
	//Client-specific functionality
	if (IsLocallyControlled())
	{
		FString healthMessage = FString::Printf(TEXT("You now have %f health remaining."), CurrentHealth);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, healthMessage);

		if (CurrentHealth <= 0)
		{
			FString deathMessage = FString::Printf(TEXT("You have been killed."));
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, deathMessage);
		}
	}

	//Server-specific functionality
	if (Role == ROLE_Authority)
	{
		FString healthMessage = FString::Printf(TEXT("%s now has %f health remaining."), *GetFName().ToString(), CurrentHealth);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, healthMessage);
	}

	//Functions that occur on all machines. 
	/*
		Any special functionality that should occur as a result of damage or death should be placed here.
	*/
}

//Variables replicate any time their value changes rather than constantly replicating, and RepNotifies run any time the client successfully receives a replicated value for a variable. Therefore, any time we change the player's CurrentHealth on the server, we would expect OnRep_CurrentHealth to run on each connected client. This makes OnRep_CurrentHealth the ideal place to call OnHealthUpdate on clients' machines.
void AThirdPersonMPCharacter::OnRep_CurrentHealth()
{
	OnHealthUpdate();
}


//SetCurrentHealth provides a controlled means of modifying the player's CurrentHealth from outside of AThirdPersonMPCharacter. It is not a replicated function, but by checking that the Network Role of the Actor is ROLE_Authority, we restrict this function to execute only if it is called on the server that is hosting the game. It clamps CurrentHealth to values between 0 and the player's MaxHealth, making it impossible to set CurrentHealth to an invalid value, and it also calls OnHealthUpdate to ensure that the server and clients both have parallel calls to this function. This is necessary because the server will not receive the RepNotify.
//While "setter" functions like this are not necessary for every variable, they are preferable for sensitive gameplay variables that change frequently during play, especially if they can be modified by many different sources.This is a best - practice for single - player and multiplayer games alike, as it makes live changes to these variables more consistent, easier to debug, and easier to extend with new functionality.
void AThirdPersonMPCharacter::SetCurrentHealth(float healthValue)
{
	if (Role == ROLE_Authority)
	{
		CurrentHealth = FMath::Clamp(healthValue, 0.f, MaxHealth);
		OnHealthUpdate();
	}
}


//The built - in functions for applying damage to Actors call the basic TakeDamage function for that Actor.In this case we implement a simple health deduction using SetCurrentHealth.
float AThirdPersonMPCharacter::TakeDamage(float DamageTaken, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float damageApplied = CurrentHealth - DamageTaken;
	SetCurrentHealth(damageApplied);
	return damageApplied;
}


