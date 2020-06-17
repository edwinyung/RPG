// Fill out your copyright notice in the Description page of Project Settings.


#include "ThirdPersonMPProjectile.h"

//The first four are the components we are using while GamePlayStatics.h will give us access to basic gameplay functions, and ConstructorHelpers.h will give us access to some useful Constructor functions for setting up our components.
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Particles/ParticleSystem.h"

#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
AThirdPersonMPProjectile::AThirdPersonMPProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//The bReplicates variable tells the game that this Actor should replicate. By default, the Actor would only exist locally on the machine that spawns it. With bReplicates set to True, as long as an authoritative copy of the Actor exists on the server, it will try to replicate the Actor to all connected clients.
	bReplicates = true;

	//These will initialize both the amount of Damage that the Projectile will deal to an Actor as well as the Damage Type that will be used in the damage event. Here we are initializing with the base UDamageType, as we have not yet defined any new Damage Types.
	DamageType = UDamageType::StaticClass();
	Damage = 10.0f;

	//This will set the asset reference for our ExplosionEffect to be the P_Explosion asset inside of StarterContent.
	static ConstructorHelpers::FObjectFinder<UParticleSystem> DefaultExplosionEffect(TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion"));
	if (DefaultExplosionEffect.Succeeded())
	{
		ExplosionEffect = DefaultExplosionEffect.Object;
	}

	//Definition for the SphereComponent that will serve as the Root component for the projectile and its collision.
	//This will define the SphereComponent when the object is constructed, giving our Projectile collision.
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("RootComponent"));
	SphereComponent->InitSphereRadius(37.5f);
	SphereComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = SphereComponent;

	//Registering the Projectile Impact function on a Hit event.
	//This will register the OnProjectileImpact function with the OnComponentHit event on the Sphere Component, which acts as the projectile's primary collision component. To make especially sure that only the server runs this gameplay logic, we check for Role == ROLE_Authority before registering OnProjectileImpact.
	//underneath the line that reads RootComponent = SphereComponent
	if (Role == ROLE_Authority)
	{
		SphereComponent->OnComponentHit.AddDynamic(this, &AThirdPersonMPProjectile::OnProjectileImpact);
	}

	//This will define the StaticMeshComponent that we are using as a visual representation. It will automatically try to find the Shape_Sphere mesh inside of StarterContent and fill it in for us. The sphere will also be scaled so as to align with our SphereComponent in size.
	//Definition for the Mesh that will serve as our visual representation.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Game/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	StaticMesh->SetupAttachment(RootComponent);

	//Set the Static Mesh and its position/scale if we successfully found a mesh asset to use.
	if (DefaultMesh.Succeeded())
	{
		StaticMesh->SetStaticMesh(DefaultMesh.Object);
		StaticMesh->RelativeLocation = FVector(0.0f, 0.0f, -37.5f);
		StaticMesh->RelativeScale3D = FVector(0.75f, 0.75f, 0.75f);
	}

	//Definition for the Projectile Movement Component.
	//This will define the Projectile Movement Component for our Projectile. This Component is replicated, and any movement that it performs on the server will be reproduced on clients.
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovementComponent->SetUpdatedComponent(SphereComponent);
	ProjectileMovementComponent->InitialSpeed = 1500.0f;
	ProjectileMovementComponent->MaxSpeed = 1500.0f;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->ProjectileGravityScale = 0.0f;

}

// Called when the game starts or when spawned
void AThirdPersonMPProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}


//This is the function that we are going to call when the Projectile impacts with an object. If the object it impacts with is a valid Actor, it will call the ApplyPointDamage function to damage it at the point where the collision takes place. Meanwhile, any collision regardless of the impacted surface will destroy this Actor, causing the explosion effect to appear.
void AThirdPersonMPProjectile::OnProjectileImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor)
	{
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage, NormalImpulse, Hit, Instigator->Controller, this, DamageType);
	}

	Destroy();
}

//The Destroyed function is called any time an Actor is destroyed. Particle emitters themselves do not normally replicate, but since Actor destruction does replicate, we know that if we destroy this projectile on the server then this function will be called on each connected client when they destroy their own copies of it. As a result, all players will see the explosion effect when the projectile is destroyed.
void AThirdPersonMPProjectile::Destroyed()
{
	FVector spawnLocation = GetActorLocation();
	UGameplayStatics::SpawnEmitterAtLocation(this, ExplosionEffect, spawnLocation, FRotator::ZeroRotator, true, EPSCPoolMethod::AutoRelease);
}

// Called every frame
void AThirdPersonMPProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

