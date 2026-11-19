#pragma once
// GraphQL operations the device sends to the Remi API (validated against backend/schema.graphql by tools/check_queries.mjs).
static const char Q_LOGIN[] =
    "mutation Login($e:String!,$p:String!){login(input:{email:$e,password:$p}){accessToken refreshToken expiresInSeconds}}";
static const char Q_REFRESH[] =
    "mutation Refresh($t:String!){refreshAccessToken(input:{refreshToken:$t}){accessToken refreshToken expiresInSeconds}}";
static const char Q_WHOAMI[] = "query Who{me{name} myHouseholds{household{id}}}";
static const char Q_INVENTORY[] =
    "query Kitchen($after:String){inventory(first:50,after:$after){nodes{foodId foodName quantity unit estimatedExpiryAt} "
    "hasNextPage endCursor}}";
static const char Q_SHOPPING[] = "query Shopping{shoppingLists{id status items{id foodId foodName quantity unit status}}}";
static const char Q_IDEAS[] =
    "query Ideas{recipeSuggestions(first:6){nodes{haveCount requiredCount recipe{id name cookTimeMin prepTimeMin}}}}";
static const char Q_RECIPE[] =
    "query Recipe($id:ID!){recipe(id:$id){id name cookTimeMin prepTimeMin steps{stepNumber instruction durationMin} "
    "ingredients{foodId quantity unit optional}}}";
static const char Q_TODAY[] =
    "query Today($d:String!){mealPlans(from:$d,to:$d){meals{mealDate mealType recipeId recipeName}} "
    "dailyNutrition(from:$d,to:$d){calories proteinG mealCount} myProfile{goal{calorieTarget proteinTargetG}}}";
static const char Q_FOODS[] = "query Foods($q:String!){foods(search:$q,first:6){nodes{id name defaultUnit}}}";
static const char Q_ADD_STOCK[] = "mutation AddStock($i:InventoryStockInput!){addStock(input:$i){__typename}}";
static const char Q_CONSUME[] = "mutation Use($i:InventoryStockInput!){consumeStock(input:$i){__typename}}";
static const char Q_DISCARD[] = "mutation Toss($i:InventoryStockInput!){discardStock(input:$i){__typename}}";
static const char Q_LOG_MEAL[] = "mutation Cooked($i:LogMealInputType!){logMeal(input:$i){__typename}}";
static const char Q_ADD_TO_LIST[] = "mutation AddToList($i:AddToShoppingListInputType!){addToShoppingList(input:$i){id}}";
static const char Q_START_PAIRING[] =
    "mutation Pair($n:String!){startDevicePairing(deviceName:$n){pairingId code secret expiresInSeconds}}";
static const char Q_CLAIM_PAIRING[] =
    "mutation Claim($id:String!,$s:String!){claimDevicePairing(pairingId:$id,secret:$s){status refreshToken householdId}}";
static const char Q_MARK_BOUGHT[] =
    "mutation Bought($i:UpdateShoppingItemInputType!){updateShoppingListItem(input:$i){id status}}";
