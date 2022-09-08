const enableCheckbox = $(".enable-checkbox")
const body = $("body")
const form = $("form");

const editedFields = {}

body.on("click","input[type=checkbox]",(e)=>{
    const open = e.target.checked
    const dependentNames = e.target.dataset.dependents.split(";");
    dependentNames.forEach(name=>{
        const switchElement = document.querySelector(`.input-group[name=${name}]`)
        if(open){
            switchElement.classList.add("on")
        }else{
            switchElement.classList.remove("on")
        }
    })
    saveField(e.target.name,open)
})

body.on("click",".edit-btn",(e)=>{
    try{
        const getSetting = (element)=>{
            if(element.getAttribute("name")!==null){
                return (element.getAttribute("name"))
            }
                return getSetting(element.parentElement)

        }
        const setting = getSetting(e.target)

        const btn = document.querySelector(`.input-group[name='${setting}'] .edit-btn`)
        btn.classList.remove("on")
        const enabledBtns = document.querySelectorAll(`.reset-btn[data-setting=${setting}],.save-btn[data-setting=${setting}]`)
        enabledBtns.forEach(btn=>btn.classList.add("on"))
        enableInputField(setting)
    }catch(err){
        console.log(err);
    }
})

body.on("click",".reset-btn, .save-btn",(e)=>{
    try{
        const btn = e.currentTarget;
        const setting = btn.dataset.setting
        const disabledBtns = document.querySelectorAll(`.reset-btn[data-setting=${setting}],.save-btn[data-setting=${setting}]`)
        disabledBtns.forEach(btn=>btn.classList.remove("on"))
        document.querySelector(`.edit-btn[data-setting=${setting}]`).classList.add("on")
        disableInputField(setting)
        const input = document.querySelector(`input[name=${setting}]`)
        if (btn.classList.contains("save-btn")){
            saveField(setting,input.value)
        }else{
            resetField(setting)
        }
    }catch(err){
        console.log(err);
    }
})

form.on("submit",(e)=>{
    e.preventDefault();
})

async function updateSetting(setting,value){
    const resp = await fetch("/settings",{
        method:"POST",
        body:JSON.stringify({setting,value}),
        headers:{
            "Content-Type": "application/json"
        }
    })
}

document.querySelectorAll("input").forEach(input=>{
    if (input.value=="true"){
        input.setAttribute("checked",true)
    }
    if (input.type!=="checkbox"){
        input.setAttribute("disabled",true);
        input.classList.add("masked-input")
    }

})

function enableInputField(setting){
    const input = document.querySelector(`input[name=${setting}]`)
    input.removeAttribute("disabled")
    input.classList.remove("masked-input")
}
function disableInputField(setting){
    const input = document.querySelector(`input[name=${setting}]`)
    input.setAttribute("disabled",true)
    input.classList.add("masked-input")
}

function resetField(setting){
    const input = document.querySelector(`input[name=${setting}]`);
    input.setAttribute("value",editedFields[setting])
    input.value = editedFields[setting]
}

function saveField(setting,value){
    editedFields[setting]= value
    updateSetting(setting,value)
}

