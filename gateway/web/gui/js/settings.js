const enableCheckbox = $(".enable-checkbox")
const body = $("body")
const form = $("form")

body.on("click",".enable-checkbox",(e)=>{
    const disabled = !e.target.checked
    const inputId = e.target.name+"-input"
    const input = $(`#${inputId}`)
    input.attr("disabled",disabled)
    if(!disabled){
        input.val("")
    }else{
        input.val("Disabled")
    }
})

form.on("submit",(e)=>{
    e.preventDefault()
    const inputs = document.querySelectorAll("form input")
    const formValues = {};
    inputs.forEach((input)=>{
        formValues[input.name] = input.value;
    })
    updateSettings(formValues)
})

async function updateSettings(formValues){
    const resp = await fetch(window.location.pathname,{
        method:"POST",
        body:JSON.stringify(formValues),
        headers:{
            "Content-Type": "application/json"
        }
    })
}